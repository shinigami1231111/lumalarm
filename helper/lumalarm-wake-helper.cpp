// Lumalarm wake helper — org.lumalarm.Helper, a D-Bus *system* service.
//
// Single-purpose, root-run replacement for the old sudoers NOPASSWD rule:
// the desktop app no longer executes rtcwake with elevated privileges —
// it asks this helper over the system bus instead.
//
// Contract (data/org.lumalarm.Helper.xml):
//   ScheduleWake(u seconds) -> (b success, s message)
//   CancelWake()            -> (b success, s message)
//   GetStatus()             -> (s status)   idle | scheduled | scheduled:<n> | error
//
// Security rules kept deliberately narrow:
//   - seconds must satisfy WakePolicy::isValidWakeDelay() i.e. [60, 86400],
//     checked from the shared header src/wake/WakeSecurity.h BEFORE anything
//     else, so the app and the helper can never enforce different bounds.
//   - systemd >= 254: transient timer  systemd-run --on-active=<N>s
//     --timer-property=WakeSystem=yes --description=lumalarm-wake-seconds=<N>
//     --unit=lumalarm-wake-<uuid> /bin/true
//     (the UUID suffix prevents two schedules from clobbering each other).
//   - otherwise:     rtcwake -m no -s <N>
//   - QProcess argument lists ONLY — no system(), no shell, no string
//     interpolation into a shell, no configuration, no file I/O.
//   - every accepted and rejected call is logged with the caller's UID.
//   - privilege gate: uid 0 passes (fast path); every other caller must
//     satisfy the polkit action org.lumalarm.schedule.wake (auth_admin_keep),
//     checked through `pkcheck --action-id … --system-bus-name <name>`
//     (or --process <pid>) WITH --allow-user-interaction so the desktop
//     polkit agent can actually run the challenge the policy describes.
//
// State (STG-1.4): this process keeps NO authoritative state of its own. The
// armed transient timer in systemd — enumerated by its lumalarm-wake-* name
// and read back through its Description= label — is the single source of
// truth, so a helper restart still reports and cancels a wake that is
// already armed.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDebug>
#include <QProcess>
#include <QStringList>
#include <QUuid>

#include "wake/WakeSecurity.h"

namespace {
constexpr int kSystemdRunMinVersion = 254; // WakeSystem= transient timers

// Upper bound on one pkcheck run. Generous because --allow-user-interaction
// may hold the call open while a human answers the polkit dialog; bounded so
// a helper with no reachable agent cannot be wedged forever. Must stay BELOW
// the client's kHelperCallTimeoutMs (70 s) so the client never gives up first
// and double-arms on its direct-rtcwake fallback.
constexpr int kPkcheckTimeoutMs = 60000;

// systemctl --version, parsed once: first line is "systemd <major> (...)".
int systemdMajorVersion()
{
    static int cached = -1; // -1 unknown, 0 unreadable
    if (cached >= 0)
        return cached;

    cached = 0;
    QProcess p;
    p.start(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--version"));
    if (p.waitForStarted(3000) && p.waitForFinished(3000) &&
        p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
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
    explicit WakeHelper(QObject *parent = nullptr) : QObject(parent) {}

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
    // `pkcheck` (QProcess argument list only — fail-closed when pkcheck is
    // unavailable or errors).
    bool authorize(uint uid, QString *reason) const;

    bool runCommand(const QString &program, const QStringList &args, QString *error,
                    int timeoutMs = 20000) const;

    // Base names (no .timer) of every armed lumalarm-wake-*.timer in systemd,
    // enumerated fresh on every call — never from this process's memory.
    QStringList armedTimerBasesFromSystemd() const;
    // Description= of a transient timer, used to recover the requested delay.
    QString timerDescription(const QString &unitBase) const;
    // Adopt whatever systemd says is armed as this instance's state. Called
    // before status reporting so a restart cannot report "idle" for a wake
    // that is still armed.
    void rediscoverState();

    // Stop `<base>.timer` (and its service/failed state). True only when the
    // timer is really gone afterwards.
    bool stopTimer(const QString &unitBase, QString *error);

    QString m_state = QStringLiteral("idle"); // "idle" | "scheduled" | "error"
    QString m_lastError;
    QString m_unitName; // transient timer unit base name, systemd path
    bool m_usedRtcwake = false;
    uint m_seconds = 0; // accepted request, echoed by GetStatus
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

// ── Systemd state discovery (STG-1.4) ────────────────────────────────────────
//
// The helper deliberately keeps no file state (see the security rules above),
// so the transient timer units themselves carry everything: their names are
// prefixed lumalarm-wake- and their Description= holds the requested delay.

QStringList WakeHelper::armedTimerBasesFromSystemd() const
{
    const QStringList args{
        QStringLiteral("list-units"),
        QStringLiteral("--all"),
        QStringLiteral("--no-legend"),
        QStringLiteral("--plain"),
        QStringLiteral("--full"),
        QStringLiteral("%1*.timer").arg(QLatin1String(WakePolicy::kTimerPrefix)),
    };
    QProcess p;
    p.start(QStringLiteral("systemctl"), args); // argument list — never a shell
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000))
        return QStringList();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return QStringList();
    return WakePolicy::armedTimerBases(QString::fromUtf8(p.readAllStandardOutput()));
}

QString WakeHelper::timerDescription(const QString &unitBase) const
{
    const QStringList args{
        QStringLiteral("show"),
        unitBase + QStringLiteral(".timer"),
        QStringLiteral("--property=Description"),
        QStringLiteral("--value"),
    };
    QProcess p;
    p.start(QStringLiteral("systemctl"), args);
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000))
        return QString();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return QString();
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

void WakeHelper::rediscoverState()
{
    const QStringList bases = armedTimerBasesFromSystemd();

    if (!bases.isEmpty()) {
        const QString base = bases.first();
        if (bases.size() > 1)
            qWarning() << "[wake-helper]" << bases.size() << "armed Lumalarm timers found;"
                       << "adopting" << base;

        qint64 labelledSeconds = 0;
        const bool labelled =
            WakePolicy::secondsFromTimerLabel(timerDescription(base), &labelledSeconds);

        if (m_unitName != base)
            qInfo() << "[wake-helper] rediscovered armed wake timer" << base
                    << (labelled ? QString("label %1 s").arg(labelledSeconds)
                                 : QStringLiteral("no label"));

        m_unitName = base;
        m_usedRtcwake = false;
        m_state = QStringLiteral("scheduled");
        if (labelled)
            m_seconds = static_cast<uint>(labelledSeconds);
        return;
    }

    // Nothing armed in systemd. Only touch state that belongs to a systemd
    // timer — an rtcwake-based wake this instance armed is not visible here
    // and must survive.
    if (!m_unitName.isEmpty()) {
        qInfo() << "[wake-helper] timer" << m_unitName
                << "is gone (fired or stopped externally) -> idle";
        m_unitName.clear();
        if (m_state == QLatin1String("scheduled")) {
            m_state = QStringLiteral("idle");
            m_seconds = 0;
            m_lastError.clear();
        }
    }
}

bool WakeHelper::stopTimer(const QString &unitBase, QString *error)
{
    // D5: use the fully-qualified .timer name — a bare `systemctl stop
    // lumalarm-wake-<uuid>` resolves to <base>.service, "succeeds" (the
    // inactive service stops fine) and leaves the timer armed.
    if (!runCommand(QStringLiteral("systemctl"),
                    QStringList() << QStringLiteral("stop") << unitBase + QStringLiteral(".timer"),
                    error))
        return false;

    // Best-effort cleanup of the leftover service unit and any failed-unit
    // state; not fatal if they are already gone.
    runCommand(QStringLiteral("systemctl"),
               QStringList() << QStringLiteral("stop") << unitBase + QStringLiteral(".service"),
               nullptr);
    runCommand(QStringLiteral("systemctl"),
               QStringList() << QStringLiteral("reset-failed")
                             << unitBase + QStringLiteral(".timer")
                             << unitBase + QStringLiteral(".service"),
               nullptr);
    return true;
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

    // Identify the caller the way polkit expects: prefer the unique bus name
    // (owned and tracked by the bus daemon, so it cannot be recycled or
    // spoofed the way a bare PID can), falling back to the bus-tracked PID
    // when the message carries no sender.
    const QString busName = calledFromDBus() ? message().service() : QString();
    const uint pid = callerPid();
    if (uid == ~0u || (busName.isEmpty() && pid == ~0u)) {
        if (reason)
            *reason = QStringLiteral("not authorized: unknown caller");
        return false;
    }

    // --allow-user-interaction is what makes the policy reachable at all:
    // data/org.lumalarm.schedule.policy declares allow_active=auth_admin_keep,
    // so without it pkcheck refuses to contact the desktop authentication
    // agent and every non-root caller is denied forever with
    // "Authorization requires authentication and -u wasn't passed".
    // The arguments (including the action id) come from the shared STG-1
    // header so they cannot drift from what the app and tests expect.
    QProcess p;
    p.start(QStringLiteral("pkcheck"), WakePolicy::pkcheckArguments(busName, pid));
    if (!p.waitForStarted(5000)) {
        if (reason)
            *reason =
                QStringLiteral("not authorized: pkcheck unavailable: %1").arg(p.errorString());
        return false;
    }
    if (!p.waitForFinished(kPkcheckTimeoutMs)) {
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

bool WakeHelper::runCommand(const QString &program, const QStringList &args, QString *error,
                            int timeoutMs) const
{
    QProcess p;
    p.start(program, args); // explicit argument list — never a shell
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

    // 1. Range gate — the only input the caller controls, checked FIRST and
    //    through the same shared function the app validates with (STG-1.3).
    if (!WakePolicy::isValidWakeDelay(seconds)) {
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
        // Replace semantics, taken from systemd rather than from this
        // process's memory: enumerate what is actually armed (this also
        // covers a helper that restarted after its predecessor scheduled a
        // wake) and disarm all of it, so at most one Lumalarm wake exists.
        const QStringList existing = armedTimerBasesFromSystemd();
        for (const QString &base : existing) {
            QString stopErr;
            stopTimer(base, &stopErr); // failure is detected by the re-check below
        }
        if (!armedTimerBasesFromSystemd().isEmpty()) {
            m_state = QStringLiteral("error");
            m_lastError = QStringLiteral("could not disarm the previous Lumalarm wake");
            message = m_lastError;
            qWarning() << "[wake-helper] ScheduleWake: uid" << uid << "-> REJECTED:" << message;
            return false;
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
            // The label is the restart-safe copy of the requested delay: it
            // lives in systemd, so GetStatus() can still report the real
            // number after this process is gone (STG-1.4).
            QStringLiteral("--description=%1").arg(WakePolicy::timerLabel(seconds)),
            QStringLiteral("--unit=%1").arg(unit), QStringLiteral("/bin/true")};
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
    //     Known limitation: the RTC carries no Lumalarm marker, so a restart
    //     on systemd < 254 cannot re-attribute this wake (the helper does no
    //     file I/O by design). The systemd path above is the supported one.
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

    // Enumerate from systemd first (STG-1.4): after a helper restart the
    // armed timer is not in this process's memory, and answering "idle" here
    // would be a false success while the wake is still live. Stop everything
    // found, not just the unit this instance remembers.
    const QStringList bases = armedTimerBasesFromSystemd();
    if (!bases.isEmpty()) {
        for (const QString &base : bases) {
            QString stopErr;
            if (!stopTimer(base, &stopErr)) {
                // Truthful failure: the timer is (still) armed.
                m_state = QStringLiteral("error");
                m_lastError = stopErr;
                message = stopErr;
                qWarning() << "[wake-helper] CancelWake: uid" << uid
                           << "-> REJECTED (stop failed for" << base << "):" << stopErr;
                return false;
            }
        }
        if (!armedTimerBasesFromSystemd().isEmpty()) {
            m_state = QStringLiteral("error");
            m_lastError = QStringLiteral("timer still armed after stop");
            message = m_lastError;
            qWarning() << "[wake-helper] CancelWake: uid" << uid << "-> REJECTED:" << message;
            return false;
        }
        m_unitName.clear();
        m_usedRtcwake = false;
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        message = QStringLiteral("cancelled");
        qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (systemd timer"
                << (bases.size() > 1 ? QStringLiteral("s") : QString()) << "stopped)";
        return true;
    }

    // Path B: rtcwake was used for this schedule.
    if (m_usedRtcwake) {
        QString err;
        if (!runCommand(QStringLiteral("rtcwake"),
                        QStringList() << QStringLiteral("-m") << QStringLiteral("disable"), &err)) {
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

    // Nothing armed anywhere — idempotent success, and any leftover error
    // state from an earlier failed call is cleared so GetStatus() reports
    // "idle" after a successful cancel.
    m_unitName.clear();
    m_state = QStringLiteral("idle");
    m_lastError.clear();
    m_seconds = 0;
    message = QStringLiteral("idle");
    qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (idle, nothing to cancel)";
    return true;
}

QString WakeHelper::GetStatus()
{
    // STG-1.4: ask systemd, not this process, so a helper restart cannot
    // report "idle" for a wake that is still armed.
    rediscoverState();

    const uint uid = callerUid();
    if (m_state == QLatin1String("scheduled")) {
        const QString status = m_seconds > 0
                                   ? QStringLiteral("scheduled:") + QString::number(m_seconds)
                                   : QStringLiteral("scheduled");
        qInfo() << "[wake-helper] GetStatus: uid" << uid << "->" << status;
        return status;
    }

    // STG-1.7/F7: GetStatus stays public and read-only — it performs no
    // privileged operation, and the fact that a Lumalarm wake is armed is
    // already world-readable from `systemctl list-timers 'lumalarm-wake-*.timer'`,
    // so gating it on polkit would buy nothing while breaking unauthenticated
    // diagnostics. What is NOT public is the free-form error detail: only the
    // state word crosses the bus to an unauthenticated caller, and the full
    // text stays in the root-only journal.
    const QString status =
        m_state == QLatin1String("error") ? QStringLiteral("error") : QStringLiteral("idle");
    qInfo() << "[wake-helper] GetStatus: uid" << uid << "->" << status;
    return status;
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
    if (!bus.registerService(QString::fromLatin1(WakePolicy::kHelperService))) {
        qCritical() << "[wake-helper] failed to register" << WakePolicy::kHelperService << ':'
                    << bus.lastError().message()
                    << "(is the send/own policy installed in /usr/share/dbus-1/system.d/?)";
        return 1;
    }
    bus.registerObject(QString::fromLatin1(WakePolicy::kHelperPath), &helper,
                       QDBusConnection::ExportAllSlots);

    qInfo() << "[wake-helper] org.lumalarm.Helper ready on the system bus";
    return app.exec();
}

#include "lumalarm-wake-helper.moc"
