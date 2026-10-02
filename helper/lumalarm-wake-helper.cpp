// Lumalarm wake helper — org.lumalarm.Helper, a D-Bus *system* service.
//
// Single-purpose, root-run replacement for the old sudoers NOPASSWD rule:
// the desktop app no longer executes rtcwake with elevated privileges —
// it asks this helper over the system bus instead.
//
// Contract (data/org.lumalarm.Helper.xml):
//   ScheduleWake(u seconds) -> (b success, s message)
//   CancelWake()            -> (b success, s message)
//   GetStatus()             -> (s status)   idle | scheduled:<n> | error:<msg>
//
// Security rules kept deliberately narrow:
//   - seconds must be in [60, 86400]; anything else is rejected.
//   - systemd >= 254: transient timer  systemd-run --on-active=<N>s
//     --property=WakeSystem=yes --unit=lumalarm-wake-<uuid> /bin/true
//     (the UUID suffix prevents two schedules from clobbering each other).
//   - otherwise:     rtcwake -m no -s <N>
//   - QProcess argument lists ONLY — no system(), no shell, no string
//     interpolation into a shell, no configuration, no file I/O.
//   - every accepted and rejected call is logged with the caller's UID.
//   - privilege gate: uid 0 passes; every other caller must satisfy the
//     polkit action org.lumalarm.schedule.wake (auth_admin_keep), checked
//     through `pkcheck --action-id … --process <pid>` (argument list only).
//     The helper itself performs no file I/O and never uses a shell.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDebug>
#include <QProcess>
#include <QStringList>
#include <QUuid>

namespace {
constexpr auto kServiceName = "org.lumalarm.Helper";
constexpr auto kObjectPath = "/org/lumalarm/Helper";
constexpr uint kMinSeconds = 60;      // ScheduleWake range, both ends enforced
constexpr uint kMaxSeconds = 86400;
constexpr int kSystemdRunMinVersion = 254;   // WakeSystem= transient timers

// systemctl --version, parsed once: first line is "systemd <major> (...)".
int systemdMajorVersion()
{
    static int cached = -1;   // -1 unknown, 0 unreadable
    if (cached >= 0)
        return cached;

    cached = 0;
    QProcess p;
    p.start(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--version"));
    if (p.waitForStarted(3000) && p.waitForFinished(3000)
        && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
        const QString firstLine =
            QString::fromUtf8(p.readAllStandardOutput()).section(QLatin1Char('\n'), 0, 0);
        const QStringList parts = firstLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2 && parts.at(0) == QLatin1String("systemd")) {
            bool ok = false;
            const int major = parts.at(1).toInt(&ok);
            if (ok)
                cached = major;
        }
    }
    if (cached == 0)
        qWarning() << "[wake-helper] cannot determine systemd version;"
                   << "systemd-run timer path unavailable";
    else
        qInfo() << "[wake-helper] systemd major version:" << cached;
    return cached;
}
} // namespace

// D4: Q_CLASSINFO required. Without it, Qt exports the interface
// as local.<class> and D-Bus clients fail to resolve it.
class WakeHelper : public QObject, protected QDBusContext
{
    Q_OBJECT
    // Export the slots under the real bus interface name instead of Qt's
    // default local.<binary>.<class> — clients (WakeManager, busctl) look
    // up org.lumalarm.Helper.ScheduleWake/CancelWake/GetStatus.
    Q_CLASSINFO("D-Bus Interface", "org.lumalarm.Helper")

public:
    explicit WakeHelper(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public slots:
    // Out-args follow Qt's D-Bus convention (verified against the
    // qdbusxml2cpp-generated adaptor for data/org.lumalarm.Helper.xml):
    // return value = first OUT arg, non-const reference = second OUT arg.
    bool ScheduleWake(uint seconds, QString &message);
    bool CancelWake(QString &message);
    QString GetStatus();

private:
    // Caller's UID as tracked by the bus daemon; ~0u when not called via D-Bus.
    uint callerUid() const;
    // Caller's PID as tracked by the bus daemon; ~0u when unavailable.
    uint callerPid() const;
    // Polkit gate for the privileged operations. uid 0 passes directly;
    // everyone else must satisfy org.lumalarm.schedule.wake, checked with
    // `pkcheck --action-id <action> --process <pid>` (QProcess argument
    // list only — fail-closed when pkcheck is unavailable or errors).
    bool authorize(uint uid, QString *reason) const;
    bool runCommand(const QString &program, const QStringList &args,
                    QString *error, int timeoutMs = 20000) const;

    QString m_state = QStringLiteral("idle");  // "idle" | "scheduled" | "error"
    QString m_lastError;
    QString m_unitName;      // transient timer unit, systemd path
    bool m_usedRtcwake = false;
    uint m_seconds = 0;      // accepted request, echoed by GetStatus
};

uint WakeHelper::callerUid() const
{
    if (!calledFromDBus() || message().service().isEmpty())
        return ~0u;
    QDBusConnectionInterface *busInterface = connection().interface();
    if (!busInterface)
        return ~0u;
    const QDBusReply<uint> uid = busInterface->serviceUid(message().service());
    return uid.isValid() ? uid.value() : ~0u;
}

uint WakeHelper::callerPid() const
{
    if (!calledFromDBus() || message().service().isEmpty())
        return ~0u;
    QDBusConnectionInterface *busInterface = connection().interface();
    if (!busInterface)
        return ~0u;
    const QDBusReply<uint> pid = busInterface->servicePid(message().service());
    return pid.isValid() ? pid.value() : ~0u;
}

// D2: Polkit gate. The D-Bus policy file alone is not sufficient —
// without an explicit pkcheck here, any local process in the same
// session can call ScheduleWake without authentication.
bool WakeHelper::authorize(uint uid, QString *reason) const
{
    // Root bypasses — the helper itself runs as root and root-run clients
    // need no interactive authentication.
    if (uid == 0)
        return true;

    const uint pid = callerPid();
    if (uid == ~0u || pid == ~0u) {
        if (reason)
            *reason = QStringLiteral("not authorized: unknown caller");
        return false;
    }

    // Polkit, non-interactive: uid != 0 must satisfy
    // org.lumalarm.schedule.wake (auth_admin_keep). No agent is contacted
    // (pkcheck flags default to no interaction), so headless callers are
    // denied and a desktop agent can still authenticate the user.
    QProcess p;
    p.start(QStringLiteral("pkcheck"),
            QStringList() << QStringLiteral("--action-id")
                          << QStringLiteral("org.lumalarm.schedule.wake")
                          << QStringLiteral("--process") << QString::number(pid));
    if (!p.waitForStarted(5000)) {
        if (reason)
            *reason = QStringLiteral("not authorized: pkcheck unavailable: %1")
                          .arg(p.errorString());
        return false;
    }
    if (!p.waitForFinished(15000)) {
        p.kill();
        p.waitForFinished(2000);
        if (reason)
            *reason = QStringLiteral("not authorized: pkcheck timed out");
        return false;
    }
    if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0)
        return true;

    QString detail = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (detail.isEmpty())
        detail = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (detail.isEmpty())
        detail = QStringLiteral("exit code %1").arg(p.exitCode());
    if (reason)
        *reason = QStringLiteral("not authorized (polkit): %1").arg(detail);
    return false;
}

bool WakeHelper::runCommand(const QString &program, const QStringList &args,
                             QString *error, int timeoutMs) const
{
    QProcess p;
    p.start(program, args);   // explicit argument list — never a shell
    if (!p.waitForStarted(5000)) {
        if (error)
            *error = QStringLiteral("failed to start %1: %2").arg(program, p.errorString());
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        if (error)
            *error = program + QStringLiteral(" timed out");
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QString errText = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (errText.isEmpty())
            errText = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (errText.isEmpty())
            errText = QStringLiteral("exit code %1").arg(p.exitCode());
        if (error)
            *error = QStringLiteral("%1: %2").arg(program, errText);
        return false;
    }
    return true;
}

bool WakeHelper::ScheduleWake(uint seconds, QString &message)
{
    const uint uid = callerUid();

    // 1. Range gate — the only input the caller controls.
    if (seconds < kMinSeconds || seconds > kMaxSeconds) {
        message = QStringLiteral("out of range");
        qWarning() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds
                   << "-> REJECTED:" << message;
        return false;
    }

    // 2. Privilege gate — non-root callers must satisfy the polkit action
    //    org.lumalarm.schedule.wake (auth_admin_keep); uid 0 passes.
    QString denyReason;
    if (!authorize(uid, &denyReason)) {
        message = denyReason;
        qWarning() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds
                   << "-> REJECTED:" << message;
        return false;
    }

    // 3a. systemd >= 254: transient WakeSystem timer (preferred).
    if (systemdMajorVersion() >= kSystemdRunMinVersion) {
        // Replace semantics: disarm a timer still armed by an earlier call,
        // otherwise re-scheduling orphans the previous transient timer
        // (observed empirically: it stayed active after a second ScheduleWake).
        // Best-effort — the old unit may already have fired or be gone.
        if (!m_unitName.isEmpty()) {
            runCommand(QStringLiteral("systemctl"),
                       QStringList() << QStringLiteral("stop")
                                     << m_unitName + QStringLiteral(".timer"), nullptr);
            runCommand(QStringLiteral("systemctl"),
                       QStringList() << QStringLiteral("stop")
                                     << m_unitName + QStringLiteral(".service"), nullptr);
        }
        const QString unit =
            QStringLiteral("lumalarm-wake-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString err;
        const QStringList args{
            QStringLiteral("--on-active=%1s").arg(seconds),
            // NOTE: the task text says --property=WakeSystem=yes, but
            // WakeSystem= is a *timer* unit property: plain --property=
            // applies to the transient SERVICE and systemd 262 rejects the
            // assignment outright ("Unknown assignment: WakeSystem=yes",
            // no unit created). --timer-property= targets the generated
            // timer — same intent, actually functional.
            QStringLiteral("--timer-property=WakeSystem=yes"),
            QStringLiteral("--unit=%1").arg(unit),
            QStringLiteral("/bin/true")
        };
        // D3: --timer-property (not --property). systemd 262 rejects
        // --property=WakeSystem=yes with "Unknown assignment".
        if (runCommand(QStringLiteral("systemd-run"), args, &err)) {
            m_state = QStringLiteral("scheduled");
            m_unitName = unit;
            m_usedRtcwake = false;
            m_seconds = seconds;
            m_lastError.clear();
            message = QStringLiteral("scheduled");
            qInfo() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds
                    << "-> accepted (systemd timer" << unit << ")";
            return true;
        }
        m_state = QStringLiteral("error");
        m_lastError = err;
        message = err;
        qWarning() << "[wake-helper] ScheduleWake: uid" << uid
                   << "-> REJECTED (systemd-run failed):" << err;
        return false;
    }

    // 3b. Older systemd: direct rtcwake (the helper itself runs as root).
    QString err;
    if (runCommand(QStringLiteral("rtcwake"),
                   QStringList() << QStringLiteral("-m") << QStringLiteral("no")
                                 << QStringLiteral("-s") << QString::number(seconds),
                   &err)) {
        m_state = QStringLiteral("scheduled");
        m_unitName.clear();
        m_usedRtcwake = true;
        m_seconds = seconds;
        m_lastError.clear();
        message = QStringLiteral("scheduled");
        qInfo() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds
                << "-> accepted (rtcwake)";
        return true;
    }

    m_state = QStringLiteral("error");
    m_lastError = err;
    message = err;
    qWarning() << "[wake-helper] ScheduleWake: uid" << uid
               << "-> REJECTED (rtcwake failed):" << err;
    return false;
}

bool WakeHelper::CancelWake(QString &message)
{
    const uint uid = callerUid();

    // Privilege gate — cancelling a root-scheduled wake is privileged too.
    QString denyReason;
    if (!authorize(uid, &denyReason)) {
        message = denyReason;
        qWarning() << "[wake-helper] CancelWake: uid" << uid << "-> REJECTED:" << message;
        return false;
    }

    // D5: Use fully-qualified unit name (.timer) — bare `systemctl
    // stop lumalarm-wake-<uuid>` matches the .service and leaves
    // the timer armed.
    // Path A: a transient systemd timer from this helper instance.
    if (!m_unitName.isEmpty()) {
        // m_unitName is the bare systemd-run --unit= base name. The armed
        // unit is <base>.timer — stopping the bare name makes systemctl
        // resolve it to <base>.service, which "succeeds" (the inactive
        // service stops fine) while the timer stays armed. Observed
        // empirically, so target the .timer explicitly.
        QString err;
        if (!runCommand(QStringLiteral("systemctl"),
                        QStringList() << QStringLiteral("stop")
                                      << m_unitName + QStringLiteral(".timer"), &err)) {
            m_state = QStringLiteral("error");
            m_lastError = err;
            message = err;
            qWarning() << "[wake-helper] CancelWake: uid" << uid
                       << "-> REJECTED (stop failed):" << err;
            return false;
        }
        // Best-effort cleanup of the leftover service unit and any
        // failed-unit state; not fatal if they are already gone.
        runCommand(QStringLiteral("systemctl"),
                   QStringList() << QStringLiteral("stop")
                                 << m_unitName + QStringLiteral(".service"), nullptr);
        runCommand(QStringLiteral("systemctl"),
                   QStringList() << QStringLiteral("reset-failed")
                                 << m_unitName + QStringLiteral(".timer")
                                 << m_unitName + QStringLiteral(".service"), nullptr);
        m_unitName.clear();
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        message = QStringLiteral("cancelled");
        qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (systemd timer stopped)";
        return true;
    }

    // Path B: rtcwake was used for this schedule.
    if (m_usedRtcwake) {
        QString err;
        if (!runCommand(QStringLiteral("rtcwake"),
                        QStringList() << QStringLiteral("-m") << QStringLiteral("disable"),
                        &err)) {
            m_state = QStringLiteral("error");
            m_lastError = err;
            message = err;
            qWarning() << "[wake-helper] CancelWake: uid" << uid
                       << "-> REJECTED (disable failed):" << err;
            return false;
        }
        m_usedRtcwake = false;
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        message = QStringLiteral("cancelled");
        qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (rtcwake disabled)";
        return true;
    }

    // Nothing scheduled in this instance — idempotent success, and any
    // leftover error state from an earlier failed call is cleared so
    // GetStatus() reports "idle" after a successful cancel.
    m_state = QStringLiteral("idle");
    m_lastError.clear();
    m_seconds = 0;
    message = QStringLiteral("idle");
    qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (idle, nothing to cancel)";
    return true;
}

QString WakeHelper::GetStatus()
{
    qInfo() << "[wake-helper] GetStatus: uid" << callerUid() << "->" << m_state;
    if (m_state == QLatin1String("scheduled"))
        return QStringLiteral("scheduled:") + QString::number(m_seconds);
    if (m_state == QLatin1String("error"))
        return QStringLiteral("error:") + m_lastError;
    return QStringLiteral("idle");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qCritical() << "[wake-helper] cannot connect to the system bus:"
                    << bus.lastError().message();
        return 1;
    }

    WakeHelper helper;
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        qCritical() << "[wake-helper] failed to register" << kServiceName << ':'
                    << bus.lastError().message()
                    << "(is the send/own policy installed in /usr/share/dbus-1/system.d/?)";
        return 1;
    }
    bus.registerObject(QString::fromLatin1(kObjectPath), &helper,
                        QDBusConnection::ExportAllSlots);

    qInfo() << "[wake-helper] org.lumalarm.Helper ready on the system bus";
    return app.exec();
}

#include "lumalarm-wake-helper.moc"
