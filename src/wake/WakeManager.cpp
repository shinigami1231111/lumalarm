#include "wake/WakeManager.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTimeZone>
#include <unistd.h>

#include "wake/SchedulerState.h"

namespace {
bool isRoot()
{
    return geteuid() == 0;
}

// Prefix a privileged command with sudo when we are not root. `sudo -n` fails
// fast when the user would be prompted for a password, so the scheduler never
// hangs waiting on a password prompt.
QStringList privileged(const QString &program, const QStringList &args)
{
    if (isRoot())
        return QStringList() << program << args;
    return QStringList() << "sudo" << "-n" << program << args;
}
} // namespace

WakeManager::WakeManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_state("idle")
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WakeManager::onProcessFinished);
}

QString WakeManager::wakeState() const
{
    return m_state;
}

QString WakeManager::lastError() const
{
    return m_lastError;
}

void WakeManager::prepareWake(int secondsUntilAlarm, const QString &mode)
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    m_lastError.clear();
    m_state = "arming";
    emit wakeStateChanged();

    // Preferred: privileged D-Bus helper (org.lumalarm.Helper) — no sudoers
    // rule required. The legacy `sudo rtcwake` invocation below is kept as
    // the fallback when the helper is unavailable or declines the request.
    QString helperMsg;
    if (scheduleViaHelper(uint(secondsUntilAlarm), &helperMsg) == HelperResult::Scheduled) {
        qInfo() << "[WakeManager] Using D-Bus helper";
        m_helperOwnedWake = true;
        m_state = "armed";
        m_lastError.clear();
        emit wakePrepared();
        emit wakeStateChanged();
        return;
    }
    qWarning() << "[WakeManager] Falling back to direct rtcwake:" << helperMsg;
    m_helperOwnedWake = false;

    QStringList args;
    args << "-m" << mode << "-s" << QString::number(secondsUntilAlarm);
    m_process->start("sudo", QStringList() << "rtcwake" << args);
}

bool WakeManager::isRtcWakeAvailable() const
{
    const RtcCapabilities caps = inspectRtc();
    return caps.wakeAlarmSupported;
}

void WakeManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::NormalExit && exitCode == 0) {
        m_state = "armed";
        m_lastError.clear();
        emit wakePrepared();
    } else {
        m_state = "failed";
        m_lastError = m_process->readAllStandardError();
        if (m_lastError.isEmpty()) {
            m_lastError = "rtcwake failed with exit code " + QString::number(exitCode);
        }
        emit wakeError(m_lastError);
    }
    emit wakeStateChanged();
}

bool WakeManager::runSynchronous(const QString &program, const QStringList &args,
                                 int timeoutMs, QString *error, QString *output)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(5000)) {
        if (error)
            *error = "failed to start " + program + ": " + p.errorString();
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        if (error)
            *error = program + " timed out";
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QString errText = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (errText.isEmpty())
            errText = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (error)
            *error = program + " failed (exit " + QString::number(p.exitCode()) + "): " + errText;
        return false;
    }
    if (output)
        *output = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    return true;
}

WakeManager::HelperResult WakeManager::scheduleViaHelper(uint seconds, QString *message)
{
    QDBusInterface iface(QStringLiteral("org.lumalarm.Helper"),
                         QStringLiteral("/org/lumalarm/Helper"),
                         QStringLiteral("org.lumalarm.Helper"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        const QDBusError err = iface.lastError();
        if (message)
            *message = err.name() + QStringLiteral(": ") + err.message();
        qWarning() << "[WakeManager] D-Bus helper interface invalid:"
                   << err.name() << "-" << err.message();
        return HelperResult::Unavailable;
    }

    // Signature per data/org.lumalarm.Helper.xml:
    //   ScheduleWake(u seconds) -> (b success, s message)
    const QDBusMessage raw = iface.call(QStringLiteral("ScheduleWake"), seconds);
    if (raw.type() == QDBusMessage::ErrorMessage) {
        const QString errText = raw.errorName() + QStringLiteral(": ") + raw.errorMessage();
        if (message)
            *message = errText;
        if (raw.errorName() == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown")
            || raw.errorName() == QLatin1String("org.freedesktop.DBus.Error.AccessDenied")) {
            // The two cases the task calls out explicitly: helper not
            // installed/activated, or the bus/polkit denied the call.
            qWarning() << "[WakeManager] D-Bus helper unavailable:" << errText;
        } else {
            qWarning() << "[WakeManager] D-Bus helper call failed:" << errText;
        }
        return HelperResult::Unavailable;
    }

    const QDBusReply<bool> reply(raw);
    const bool success = reply.isValid() && reply.value();
    if (message)
        *message = raw.arguments().size() > 1 ? raw.arguments().at(1).toString() : QString();
    return success ? HelperResult::Scheduled : HelperResult::Declined;
}

bool WakeManager::cancelViaHelper(QString *message)
{
    QDBusInterface iface(QStringLiteral("org.lumalarm.Helper"),
                         QStringLiteral("/org/lumalarm/Helper"),
                         QStringLiteral("org.lumalarm.Helper"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        const QDBusError err = iface.lastError();
        if (message)
            *message = err.name() + QStringLiteral(": ") + err.message();
        return false;
    }

    const QDBusMessage raw = iface.call(QStringLiteral("CancelWake"));
    if (raw.type() == QDBusMessage::ErrorMessage) {
        if (message)
            *message = raw.errorName() + QStringLiteral(": ") + raw.errorMessage();
        qWarning() << "[WakeManager] D-Bus helper cancel failed:"
                   << raw.errorName() << "-" << raw.errorMessage();
        return false;
    }

    const QDBusReply<bool> reply(raw);
    if (message)
        *message = raw.arguments().size() > 1 ? raw.arguments().at(1).toString() : QString();
    if (!reply.isValid() || !reply.value())
        return false;
    return true;
}

RtcCapabilities WakeManager::inspectRtc() const
{
    RtcCapabilities caps;

    QDir sysCls("/sys/class/rtc");
    const QStringList names = sysCls.entryList(QStringList() << "rtc*",
                                               QDir::Dirs | QDir::NoDotAndDotDot);
    if (names.isEmpty()) {
        caps.error = "no RTC devices found in /sys/class/rtc";
        qWarning() << "[WakeManager] RTC capability: " << caps.error;
        return caps;
    }

    QString sysfsPath;
    for (const QString &n : names) {
        const QString path = sysCls.filePath(n);
        if (QFileInfo::exists(path + "/wakealarm")) {
            sysfsPath = path;
            break;
        }
        qInfo() << "[WakeManager] RTC" << path << "has no wakealarm interface; skipping";
    }

    if (sysfsPath.isEmpty()) {
        caps.available = true;
        caps.error = "no RTC with a wakealarm interface found in /sys/class/rtc";
        qWarning() << "[WakeManager] RTC capability: " << caps.error;
        return caps;
    }

    caps.available = true;
    caps.wakeAlarmSupported = true;
    caps.sysfsPath = sysfsPath;

    const QString rtcName = QFileInfo(sysfsPath).fileName(); // "rtc0"
    const QString devPath = "/dev/" + rtcName;
    caps.device = QFileInfo::exists(devPath) ? devPath : QString();

    QString err;
    caps.poweroffWakeSupported = runSynchronous("which", QStringList() << "rtcwake",
                                                2000, &err);
    if (!caps.poweroffWakeSupported) {
        qWarning() << "[WakeManager] rtcwake binary not found:" << err;
    }

    qInfo() << "[WakeManager] RTC device:"
            << (caps.device.isEmpty() ? QString("auto") : caps.device)
            << "| sysfs:" << sysfsPath
            << "| wakealarm:" << (caps.wakeAlarmSupported ? "yes" : "no")
            << "| rtcwake:" << (caps.poweroffWakeSupported ? "yes" : "no")
            << "| NOTE: firmware S5-wake from power-off cannot be verified from userspace";
    return caps;
}

bool WakeManager::programWakeAlarm(qint64 epochUtc, QString *error)
{
    const RtcCapabilities caps = inspectRtc();
    if (!caps.wakeAlarmSupported) {
        if (error)
            *error = caps.error.isEmpty() ? "RTC wake alarm not supported" : caps.error;
        qWarning() << "[WakeManager] ERROR: refusing to program RTC wake:" << *error;
        return false;
    }

    // Best-effort RTC<->system clock sync so the RTC alarm lands on the wall
    // time the user chose even if the clock drifted. Non-fatal on failure.
    QString syncErr;
    if (!runSynchronous("hwclock", QStringList() << "--systohc", 15000, &syncErr)
        && !isRoot()) {
        qWarning() << "[WakeManager] hwclock sync failed (non-fatal):" << syncErr;
    }

    // Preferred: privileged D-Bus helper. The helper's API takes a relative
    // delay, so convert the absolute epoch; outside the helper's accepted
    // window (or if the helper is unavailable/declines) the direct rtcwake
    // path below stays in charge.
    const qint64 deltaSeconds = epochUtc - QDateTime::currentSecsSinceEpoch();
    if (deltaSeconds >= 60 && deltaSeconds <= 86400) {
        QString helperMsg;
        if (scheduleViaHelper(uint(deltaSeconds), &helperMsg) == HelperResult::Scheduled) {
            qInfo() << "[WakeManager] Using D-Bus helper for wake in" << deltaSeconds << "s";
            m_helperOwnedWake = true;
            return true;
        }
        qWarning() << "[WakeManager] Falling back to direct rtcwake:" << helperMsg;
    } else {
        qWarning() << "[WakeManager] wake offset" << deltaSeconds
                   << "s is outside the helper range [60, 86400];"
                   << "falling back to direct rtcwake";
    }
    m_helperOwnedWake = false;

    qInfo() << "[WakeManager] Programming RTC wake alarm:" << epochUtc
            << "(" << QDateTime::fromSecsSinceEpoch(epochUtc).toUTC().toString(Qt::ISODate) << " UTC )";

    QStringList args = QStringList() << "-m" << "no" << "-t" << QString::number(epochUtc);
    if (!caps.device.isEmpty())
        args << "-d" << caps.device;

    QString output;
    QString err;
    if (!runSynchronous("rtcwake", args, 20000, &err, &output)) {
        // Fallback: write the sysfs wakealarm directly when we have the
        // privilege (root already, or sudo works without a password).
        if (isRoot()) {
            qWarning() << "[WakeManager] rtcwake failed, falling back to sysfs write:" << err;
            QFile f(caps.sysfsPath + "/wakealarm");
            if (!f.open(QIODevice::WriteOnly)) {
                if (error)
                    *error = "rtcwake failed and sysfs write failed: " + f.errorString();
                return false;
            }
            f.write(QByteArray::number(epochUtc));
            f.close();
            qInfo() << "[WakeManager] RTC wake alarm programmed via sysfs";
            return true;
        }
        if (error)
            *error = err;
        qWarning() << "[WakeManager] ERROR: RTC wake alarm programming failed:" << err;
        return false;
    }

    qInfo() << "[WakeManager] rtcwake output:" << output;
    return true;
}

bool WakeManager::verifyWakeAlarm(qint64 expectedUtc, QString *error) const
{
    // B1: verify the transient systemd timer when the D-Bus helper programmed
    // the wake instead of reading the RTC sysfs file. systemd's WakeSystem=
    // timers run on CLOCK_BOOTTIME and systemd programs the hardware RTC only
    // at suspend entry, so /sys/class/rtc/*/wakealarm cannot reflect the armed
    // wake while the system is awake (and always mismatches the expectation).
    // An active lumalarm-wake-*.timer is the authoritative evidence here.
    if (m_helperOwnedWake) {
        QProcess p;
        p.start("systemctl", {"list-timers", "--no-legend", "--no-pager", "lumalarm-wake-*.timer"});
        p.waitForFinished(2000);
        const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        const bool armed = !out.isEmpty();
        if (armed) {
            qInfo() << "[WakeManager] Wake verified via active systemd timer:" << out;
            return true;
        }
        if (error)
            *error = "no active lumalarm-wake-*.timer found for the helper-scheduled wake";
        qWarning() << "[WakeManager] ERROR:" << *error;
        return false;
    }

    // Direct rtcwake path: rtcwake writes the RTC immediately, so the
    // +/-5 s sysfs readback below remains a valid verification.
    const RtcCapabilities caps = inspectRtc();
    if (!caps.wakeAlarmSupported) {
        if (error)
            *error = caps.error;
        qWarning() << "[WakeManager] ERROR: cannot verify wake alarm:" << *error;
        return false;
    }

    QFile f(caps.sysfsPath + "/wakealarm");
    if (!f.open(QIODevice::ReadOnly)) {
        // Explicit limitation: RTC state is not readable on this platform.
        if (error)
            *error = "cannot read " + caps.sysfsPath + "/wakealarm: " + f.errorString();
        qWarning() << "[WakeManager] ERROR: wake alarm verification unavailable:" << *error;
        return false;
    }
    const QString content = QString::fromUtf8(f.readAll()).trimmed();
    f.close();

    bool ok = false;
    const qint64 actual = content.toLongLong(&ok);
    if (!ok || actual <= 0) {
        if (error)
            *error = "wakealarm is empty or unreadable: '" + content + "'";
        qWarning() << "[WakeManager] ERROR:" << *error;
        return false;
    }

    const qint64 delta = qAbs(actual - expectedUtc);
    const bool match = delta <= WakePolicy::RTC_ALARM_TOLERANCE_SECONDS;
    qInfo() << "[WakeManager] RTC wake alarm verified: expected" << expectedUtc
            << "actual" << actual << (match ? "OK" : "MISMATCH");
    if (!match) {
        if (error)
            *error = QString("RTC alarm mismatch: expected %1, got %2").arg(expectedUtc).arg(actual);
        qWarning() << "[WakeManager] ERROR:" << *error;
        return false;
    }
    return true;
}

qint64 WakeManager::readWakeAlarmValue() const
{
    const RtcCapabilities caps = inspectRtc();
    if (!caps.wakeAlarmSupported)
        return -1;
    QFile f(caps.sysfsPath + "/wakealarm");
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    const QString content = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    bool ok = false;
    const qint64 v = content.toLongLong(&ok);
    return ok ? v : -1;
}

bool WakeManager::clearWakeAlarm(QString *error)
{
    // If the helper programmed this wake, cancel it through the helper too
    // (stops the transient timer, not just the RTC). Helper failure or a
    // helper-owned wake that vanished falls through to the direct clear.
    if (m_helperOwnedWake) {
        QString helperMsg;
        if (cancelViaHelper(&helperMsg)) {
            qInfo() << "[WakeManager] Using D-Bus helper to cancel the wake";
            m_helperOwnedWake = false;
            return true;
        }
        qWarning() << "[WakeManager] Falling back to direct rtcwake:" << helperMsg;
        m_helperOwnedWake = false;
    }

    const RtcCapabilities caps = inspectRtc();
    if (!caps.wakeAlarmSupported)
        return false;
    if (isRoot()) {
        QFile f(caps.sysfsPath + "/wakealarm");
        if (!f.open(QIODevice::WriteOnly)) {
            if (error)
                *error = f.errorString();
            return false;
        }
        f.write("0");
        f.close();
        return true;
    }
    QString err;
    if (!runSynchronous("rtcwake", QStringList() << "-m" << "no" << "-t" << "0",
                        20000, &err)) {
        if (error)
            *error = err;
        return false;
    }
    return true;
}

bool WakeManager::requestPowerOff(QString *error)
{
    if (isRoot()) {
        if (runSynchronous("systemctl", QStringList() << "poweroff", 30000, error))
            return true;
    } else {
        // systemd/logind path: normally allowed for the active desktop
        // session via polkit, without sudo.
        QString logindErr;
        if (runSynchronous("systemctl", QStringList() << "poweroff", 30000, &logindErr))
            return true;
        // Fallback preserving the project's existing sudo integration.
        if (runSynchronous("sudo", QStringList() << "-n" << "systemctl" << "poweroff",
                           30000, error))
            return true;
        if (error)
            *error = "poweroff request failed (logind/polkit and sudo both denied): " + logindErr;
    }
    qWarning() << "[WakeManager] ERROR: poweroff request failed:"
               << (error ? *error : QString());
    return false;
}