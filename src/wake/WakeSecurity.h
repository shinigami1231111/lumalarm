#ifndef WAKESECURITY_H
#define WAKESECURITY_H

#include <QString>
#include <QStringList>

// ── STG-1 wake security: the single source of truth ───────────────────────────
//
// Included by BOTH the desktop app (WakeManager) and the root-run D-Bus helper
// (helper/lumalarm-wake-helper.cpp). Every path that can trigger a privileged
// wake operation validates against the bounds below, so the two sides can never
// drift apart and no call site can quietly skip the gate.
//
// This header is deliberately inert: no file I/O, no process spawning, no
// side effects. The helper's security posture depends on it staying that way.
//
// Namespace WakePolicy is shared with wake/SchedulerState.h on purpose — it is
// the project's one place for wake-related policy.

namespace WakePolicy {

// ── Validation boundary (STG-1.3) ────────────────────────────────────────────
// The only accepted wake delay, in seconds, inclusive at both ends. Enforced
// BEFORE any process is started and before any D-Bus call is made, so an
// unvalidated value can never reach rtcwake, the helper or any other
// privileged wake mechanism.
constexpr int kMinWakeSeconds = 60;
constexpr int kMaxWakeSeconds = 86400;

// Takes qint64 so signed negatives, values beyond int range and values up to
// UINT32_MAX are all evaluated without narrowing or unsigned wrap-around:
// 0, 1, 59, 86401, 100000, 4294967295, -1 and INT_MIN are all rejected.
constexpr bool isValidWakeDelay(qint64 seconds)
{
    return seconds >= kMinWakeSeconds && seconds <= kMaxWakeSeconds;
}

// rtcwake -m <mode> is caller-controlled on both the app path and the helper,
// so an allow-list — not a shape check — decides what may pass. Every value
// below was confirmed against `rtcwake --list-modes` on 2026-10-08
// (freeze standby mem disk off no on disable show): `disable` and `show` are
// not sleep states, so they are excluded. Standby sits between mem and disk
// because the user asked for that order (2026-10-08).
//
// A strict list subsumes the old "short alphabetic token" rule: no value can
// start with '-', contain a path separator, whitespace or a shell
// metacharacter, so none can change which option rtcwake sees.
inline const QStringList &wakeModeChoices()
{
    static const QStringList kModes{
        QStringLiteral("freeze"), QStringLiteral("mem"),   QStringLiteral("standby"),
        QStringLiteral("disk"),   QStringLiteral("off"),   QStringLiteral("no"),
        QStringLiteral("on")};
    return kModes;
}

inline bool isValidWakeMode(const QString &mode)
{
    return wakeModeChoices().contains(mode);
}

// ── D-Bus contract (STG-1.1) ─────────────────────────────────────────────────
inline const char *const kHelperService = "org.lumalarm.Helper";
inline const char *const kHelperPath = "/org/lumalarm/Helper";
inline const char *const kHelperInterface = "org.lumalarm.Helper";

// ── Polkit (STG-1.2) ─────────────────────────────────────────────────────────
inline const char *const kPolkitActionId = "org.lumalarm.schedule.wake";

// Arguments for `pkcheck`.
//
// --allow-user-interaction is the crux of F2: the policy is auth_admin_keep,
// so without this flag pkcheck refuses to talk to the desktop authentication
// agent and every non-root caller is permanently denied with
// "Authorization requires authentication and -u wasn't passed."
//
// The caller is identified by its unique D-Bus name whenever the helper has
// one (race-free — the bus daemon owns the name→process mapping), falling back
// to the bus-tracked PID otherwise. Never both: pkcheck accepts one subject.
inline QStringList pkcheckArguments(const QString &busName, uint pid)
{
    QStringList args;
    args << QStringLiteral("--action-id") << QString::fromLatin1(kPolkitActionId);
    if (!busName.isEmpty())
        args << QStringLiteral("--system-bus-name") << busName;
    else
        args << QStringLiteral("--process") << QString::number(pid);
    args << QStringLiteral("--allow-user-interaction");
    return args;
}

// ── Transient timer naming and labels (STG-1.4) ──────────────────────────────
// systemd-run --unit=lumalarm-wake-<uuid>; timers are enumerated with the same
// prefix, so a restarted helper can rediscover its own wake without any
// in-memory or on-disk state of its own.
inline const char *const kTimerPrefix = "lumalarm-wake-";

// The requested delay is written into the transient timer's Description=, which
// lives in systemd and therefore survives a helper restart. Parse and render
// strictly: anything that is not exactly our own label is ignored.
inline const char *const kTimerLabelPrefix = "lumalarm-wake-seconds=";

inline QString timerLabel(qint64 seconds)
{
    return QString::fromLatin1(kTimerLabelPrefix) + QString::number(seconds);
}

inline bool secondsFromTimerLabel(const QString &description, qint64 *seconds)
{
    const QString prefix = QString::fromLatin1(kTimerLabelPrefix);
    if (!description.startsWith(prefix))
        return false;
    bool ok = false;
    const qint64 value = description.mid(prefix.size()).toLongLong(&ok);
    if (!ok || value <= 0)
        return false;
    if (seconds)
        *seconds = value;
    return true;
}

// Parse `systemctl list-units --all --no-legend --plain --full <glob>` into the
// BASE names (no ".timer" suffix) of Lumalarm timers that are actually armed.
// Columns are: UNIT LOAD ACTIVE SUB DESCRIPTION — only ACTIVE == "active"
// (waiting/running) counts; inactive, failed and deactivating units do not.
inline QStringList armedTimerBases(const QString &listUnitsOutput)
{
    QStringList bases;
    const QStringList lines = listUnitsOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList cols = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (cols.size() < 4)
            continue;
        const QString &unit = cols.at(0);
        if (!unit.startsWith(QLatin1String(kTimerPrefix)))
            continue;
        if (!unit.endsWith(QLatin1String(".timer")))
            continue;
        if (cols.at(2) != QLatin1String("active"))
            continue;
        bases << unit.left(unit.size() - 6); // strip ".timer"
    }
    return bases;
}

} // namespace WakePolicy

#endif // WAKESECURITY_H
