#include "wake/WakeManager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
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
#include "wake/WakeSecurity.h"

namespace {
bool isRoot()
{
    return geteuid() == 0;
}

// How long a client waits for the privileged helper. It must exceed the
// helper's own pkcheck window (WakeHelper::kPkcheckTimeoutMs) so the helper
// can finish an interactive polkit challenge and answer, instead of the
// client giving up first and double-arming on the direct-rtcwake fallback.
constexpr int kHelperCallTimeoutMs = 70000;
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

bool WakeManager::helperOwnsWake() const
{
    return m_helperOwnedWake;
}

void WakeManager::prepareWake(int secondsUntilAlarm, const QString &mode)
{
    // ── STG-1.3: the one validation boundary, applied FIRST ────────────────
    // Checked before any process is started and before any D-Bus call, so an
    // unvalidated value can never reach sudo rtcwake (removed), direct
    // rtcwake, the helper or any other privileged wake mechanism. An invalid
    // request changes nothing: no kill, no state transition to "arming", no
    // wake programmed.
    if (!WakePolicy::isValidWakeDelay(secondsUntilAlarm)) {
        m_state = QStringLiteral("failed");
        m_lastError = QStringLiteral("wake delay %1 s is outside the allowed range [%2, %3] s")
                          .arg(secondsUntilAlarm)
                          .arg(WakePolicy::kMinWakeSeconds)
                          .arg(WakePolicy::kMaxWakeSeconds);
        qWarning() << "[WakeManager] prepareWake refused:" << m_lastError;
        emit wakeError(m_lastError);
        emit wakeStateChanged();
        return;
    }
    if (!WakePolicy::isValidWakeMode(mode)) {
        m_state = QStringLiteral("failed");
        m_lastError = QStringLiteral("invalid rtcwake mode '%1'").arg(mode);
        qWarning() << "[WakeManager] prepareWake refused:" << m_lastError;
        emit wakeError(m_lastError);
        emit wakeStateChanged();
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    m_lastError.clear();
    m_state = "arming";
    emit wakeStateChanged();

    // Preferred: privileged D-Bus helper (org.lumalarm.Helper) — no sudoers
    // rule required. STG-1.5: the fallback below is a plain, unprivileged
    // rtcwake; when the helper is unavailable or declines, this fails with an
    // honest error instead of escalating privileges.
    QString helperMsg;
    const uint seconds = static_cast<uint>(secondsUntilAlarm); // safe: validated above
    if (scheduleViaHelper(seconds, &helperMsg) == HelperResult::Scheduled) {
        qInfo() << "[WakeManager] Using D-Bus helper";
        m_helperOwnedWake = true;
        m_state = "armed";
        m_lastError.clear();
        emit wakePrepared();
        emit wakeStateChanged();
        return;
    }
    qWarning() << "[WakeManager] D-Bus helper unavailable/declined:" << helperMsg
               << "- falling back to a direct (unprivileged) rtcwake";
    m_helperOwnedWake = false;

    QStringList args;
    args << "-m" << mode << "-s" << QString::number(secondsUntilAlarm);
    m_process->start(QStringLiteral("rtcwake"), args);
}

bool WakeManager::isRtcWakeAvailable() const
{
    const RtcCapabilities caps = inspectRtc();
    return caps.wakeAlarmSupported;
}

QStringList WakeManager::armedWakeTimerBases()
{
    QProcess p;
    p.start(QStringLiteral("systemctl"),
            {QStringLiteral("list-units"), QStringLiteral("--all"),
             QStringLiteral("--no-legend"), QStringLiteral("--plain"),
             QStringLiteral("--full"),
             QStringLiteral("%1*.timer").arg(QLatin1String(WakePolicy::kTimerPrefix))});
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000)) {
        qWarning() << "[WakeManager] could not enumerate Lumalarm wake timers:"
                   << p.errorString();
        return QStringList();
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        qWarning() << "[WakeManager] systemctl list-units failed (exit"
                   << p.exitCode() << ")";
        return QStringList();
    }
    return WakePolicy::armedTimerBases(QString::fromUtf8(p.readAllStandardOutput()));
}

bool WakeManager::helperWakeStillArmed() const
{
    return !armedWakeTimerBases().isEmpty();
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
    // Defence in depth (STG-1.3): even though every caller validates first,
    // never hand the privileged service an out-of-range value.
    if (!WakePolicy::isValidWakeDelay(seconds)) {
        if (message)
            *message = QStringLiteral("internal error: %1 s is outside the allowed range [%2, %3] s")
                           .arg(seconds)
                           .arg(WakePolicy::kMinWakeSeconds)
                           .arg(WakePolicy::kMaxWakeSeconds);
        qWarning() << "[WakeManager] scheduleViaHelper refused out-of-range value:" << seconds;
        return HelperResult::Declined;
    }

    QDBusInterface iface(QString::fromLatin1(WakePolicy::kHelperService),
                         QString::fromLatin1(WakePolicy::kHelperPath),
                         QString::fromLatin1(WakePolicy::kHelperInterface),
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
    // An explicit timeout is required: the helper may hold the call open while
    // a polkit agent authenticates the user, and giving up early here would
    // wrongly report Unavailable and arm a second wake on the fallback path.
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(WakePolicy::kHelperService),
        QString::fromLatin1(WakePolicy::kHelperPath),
        QString::fromLatin1(WakePolicy::kHelperInterface),
        QStringLiteral("ScheduleWake"));
    msg << QVariant::fromValue(seconds);
    const QDBusMessage raw =
        iface.connection().call(msg, QDBus::Block, kHelperCallTimeoutMs);

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
    QDBusInterface iface(QString::fromLatin1(WakePolicy::kHelperService),
                         QString::fromLatin1(WakePolicy::kHelperPath),
                         QString::fromLatin1(WakePolicy::kHelperInterface),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        const QDBusError err = iface.lastError();
        if (message)
            *message = err.name() + QStringLiteral(": ") + err.message();
        return false;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(WakePolicy::kHelperService),
        QString::fromLatin1(WakePolicy::kHelperPath),
        QString::fromLatin1(WakePolicy::kHelperInterface),
        QStringLiteral("CancelWake"));
    const QDBusMessage raw = iface.connection().call(msg, QDBus::Block, kHelperCallTimeoutMs);
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
    // ── STG-1.3: same boundary as the seconds-based APIs, enforced FIRST ───
    // This is the last privileged wake path, so the delay it would produce
    // (epochUtc - now) has to satisfy the very same WakePolicy gate before a
    // single process is spawned. A wake in the past, one second away, or more
    // than a day away is refused without touching the RTC or the helper, so no
    // alternate path can bypass the validation. Signed on purpose: a past
    // epoch yields a negative delta and is rejected rather than wrapping.
    const qint64 requestedDelta = epochUtc - QDateTime::currentSecsSinceEpoch();
    if (!WakePolicy::isValidWakeDelay(requestedDelta)) {
        if (error)
            *error = QStringLiteral("wake delay %1 s is outside the allowed range [%2, %3] s; "
                                    "no wake was programmed")
                         .arg(requestedDelta)
                         .arg(WakePolicy::kMinWakeSeconds)
                         .arg(WakePolicy::kMaxWakeSeconds);
        qWarning() << "[WakeManager] ERROR: refusing to program RTC wake:" << *error;
        return false;
    }

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

    // Preferred: privileged D-Bus helper. Its API takes a relative delay, so
    // convert the absolute epoch (already validated above). When the helper is
    // unavailable or declines, the direct rtcwake path below stays in charge —
    // unprivileged, so it fails honestly instead of escalating (STG-1.5).
    const qint64 deltaSeconds = requestedDelta;
    QString helperMsg;
    if (scheduleViaHelper(static_cast<uint>(deltaSeconds), &helperMsg)
        == HelperResult::Scheduled) {
        qInfo() << "[WakeManager] Using D-Bus helper for wake in" << deltaSeconds << "s";
        m_helperOwnedWake = true;
        return true;
    }
    qWarning() << "[WakeManager] D-Bus helper unavailable/declined, falling back to direct"
               << "rtcwake:" << helperMsg;
    m_helperOwnedWake = false;

    qInfo() << "[WakeManager] Programming RTC wake alarm:" << epochUtc
            << "(" << QDateTime::fromSecsSinceEpoch(epochUtc).toUTC().toString(Qt::ISODate) << " UTC )";

    QStringList args = QStringList() << "-m" << "no" << "-t" << QString::number(epochUtc);
    if (!caps.device.isEmpty())
        args << "-d" << caps.device;

    QString output;
    QString err;
    if (!runSynchronous("rtcwake", args, 20000, &err, &output)) {
        // Fallback: write the sysfs wakealarm directly when we already have
        // the privilege (root). Never escalated — STG-1.5 removed sudo.
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
        const QStringList armed = armedWakeTimerBases();
        if (!armed.isEmpty()) {
            qInfo() << "[WakeManager] Wake verified via active systemd timer:" << armed;
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

qint64 WakeManager::readWakeAlarmFile(const QString &sysfsPath)
{
    if (sysfsPath.isEmpty())
        return -1;
    QFile f(sysfsPath + QStringLiteral("/wakealarm"));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    const QString content = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    bool ok = false;
    const qint64 v = content.toLongLong(&ok);
    return ok ? v : -1;
}

qint64 WakeManager::readWakeAlarmValue() const
{
    const RtcCapabilities caps = inspectRtc();
    if (!caps.wakeAlarmSupported)
        return -1;
    return readWakeAlarmFile(caps.sysfsPath);
}

bool WakeManager::clearWakeAlarm(QString *error)
{
    // ── Helper-owned wake (STG-1.4 + F6) ──────────────────────────────────
    // The helper is the only thing that can stop the transient timer it
    // created, so this branch must not silently fall through to a direct RTC
    // clear — that cannot touch the timer and would report a success while a
    // wake is still armed.
    if (m_helperOwnedWake) {
        QString helperMsg;
        if (cancelViaHelper(&helperMsg)) {
            qInfo() << "[WakeManager] Using D-Bus helper to cancel the wake";
            m_helperOwnedWake = false;
            return true;
        }

        // Cancel failed. Ownership must SURVIVE this so the next call retries:
        // dropping it here strands a live transient timer and makes every later
        // cancel skip the helper entirely (the F6 defect).
        if (helperWakeStillArmed()) {
            if (error)
                *error = helperMsg;
            qWarning() << "[WakeManager] helper cancel failed; wake still armed,"
                       << "helper ownership kept for a retry:" << helperMsg;
            return false;
        }

        // No transient timer is armed. Decide truthfully between "the wake is
        // gone" and "a wake lives in the RTC instead" before releasing
        // ownership — never a false success, never a false failure.
        const RtcCapabilities caps = inspectRtc();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 rtc = caps.wakeAlarmSupported ? readWakeAlarmFile(caps.sysfsPath) : -1;
        if (rtc <= 0 || rtc <= now) {
            qInfo() << "[WakeManager] helper cancel failed but no timer and no future RTC"
                    << "alarm remain - the wake is gone:" << helperMsg;
            m_helperOwnedWake = false;
            return true;
        }
        qWarning() << "[WakeManager] helper cancel failed; falling back to the direct RTC"
                   << "clear for the remaining alarm at" << rtc << ":" << helperMsg;
    }

    // ── Direct RTC clear ──────────────────────────────────────────────────
    // Also reached for wakes programmed without the helper. Ownership is only
    // released once this reports that nothing is left armed.
    const RtcCapabilities caps = inspectRtc();
    if (!caps.wakeAlarmSupported) {
        if (error)
            *error = caps.error.isEmpty() ? QStringLiteral("RTC wake alarm not supported")
                                          : caps.error;
        qWarning() << "[WakeManager] ERROR: cannot clear wake alarm:" << *error;
        return false;
    }
    if (isRoot()) {
        QFile f(caps.sysfsPath + "/wakealarm");
        if (!f.open(QIODevice::WriteOnly)) {
            if (error)
                *error = f.errorString();
            return false;
        }
        f.write("0");
        f.close();
        m_helperOwnedWake = false;
        return true;
    }
    QString err;
    if (!runSynchronous("rtcwake", QStringList() << "-m" << "no" << "-t" << "0",
                        20000, &err)) {
        if (error)
            *error = err;
        return false;
    }
    m_helperOwnedWake = false;
    return true;
}

bool WakeManager::requestPowerOff(QString *error)
{
    // systemd/logind only (STG-1.5). The `sudo -n systemctl poweroff` fallback
    // is gone: power-off is authorised by polkit's org.freedesktop.login1.*
    // action for the active desktop session, so a local escalation path is
    // neither needed nor permitted here.
    QString logindErr;
    if (runSynchronous("systemctl", QStringList() << "poweroff", 30000, &logindErr))
        return true;
    if (error)
        *error = QStringLiteral("poweroff request failed (logind/polkit denied): %1")
                     .arg(logindErr);
    qWarning() << "[WakeManager] ERROR: poweroff request failed:" << (error ? *error : QString());
    return false;
}

bool WakeManager::armTestWake(int seconds)
{
    // The shared STG-1.3 boundary first, then the narrower test-wake window on
    // top of it — the test window can never exceed the helper's boundary.
    if (!WakePolicy::isValidWakeDelay(seconds) || seconds > kTestWakeMaxSeconds) {
        qWarning() << "[WakeManager] armTestWake refused: out of range" << seconds;
        emit testWakeFailed(QStringLiteral("out of range"));
        return false;
    }

    QString msg;
    if (m_helperOwnedWake) {
        // Never stack test wakes: drop the previous one first. Best effort — a
        // failed cancel is logged but does not block the new request.
        QString cancelMsg;
        if (!cancelViaHelper(&cancelMsg))
            qWarning() << "[WakeManager] pre-arm cancel failed:" << cancelMsg;
    }

    const HelperResult r = scheduleViaHelper(static_cast<uint>(seconds), &msg);
    if (r != HelperResult::Scheduled) {
        qWarning() << "[WakeManager] Test wake arm failed:" << msg;
        emit testWakeFailed(msg);
        return false;
    }

    m_helperOwnedWake = true;
    m_state = QStringLiteral("armed");
    qInfo() << "[WakeManager] Test wake armed in" << seconds << "s";
    emit testWakeArmed(seconds);
    emit wakeStateChanged();
    return true;
}

void WakeManager::cancelTestWake()
{
    QString msg;
    if (cancelViaHelper(&msg)) {
        m_helperOwnedWake = false;
        m_state = QStringLiteral("idle");
        qInfo() << "[WakeManager] Test wake cancelled";
        emit wakeStateChanged();
        return;
    }
    // F6: a failed cancel must not drop ownership — unless systemd proves the
    // wake is already gone, in which case reporting "idle" is the truth.
    if (helperWakeStillArmed()) {
        qWarning() << "[WakeManager] Test wake cancel failed; wake still armed:"
                   << msg;
        return;
    }
    qWarning() << "[WakeManager] Test wake cancel failed but no Lumalarm timer is armed:"
               << msg;
    m_helperOwnedWake = false;
    m_state = QStringLiteral("idle");
    emit wakeStateChanged();
}
