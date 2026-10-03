#include "wake/Scheduler.h"

#include <QDebug>
#include <QUuid>

Scheduler::Scheduler(AlarmManager *manager, WakeManager *wake,
                     const QString &stateFilePath, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_wake(wake)
    , m_checkTimer(new QTimer(this))
    , m_lastCheckTime(QDateTime::currentDateTime())
    , m_store(stateFilePath)
    , m_wakeState("idle")
{
    m_pending = m_store.pending();

    connect(m_checkTimer, &QTimer::timeout, this, &Scheduler::onCheckTimer);
    m_checkTimer->start(1000);

    connect(m_manager, &AlarmManager::alarmsChanged, this, &Scheduler::nextAlarmChanged);
    connect(m_manager, &AlarmManager::alarmsChanged, this, &Scheduler::onAlarmsChanged);

    // Boot recovery: evaluate any pending wake armed before the previous
    // poweroff. Deferred by one event-loop turn so QML signal connections
    // (ringing overlay) exist before a recovered alarm fires. Idempotent — a
    // consumed occurrence is a no-op.
    QTimer::singleShot(0, this, &Scheduler::recoverAfterBoot);
}

QDateTime Scheduler::computeNextOccurrence(const Alarm &a, const QDateTime &from) const
{
    QTime alarmTime(a.hour, a.minute, 0);
    QDateTime now = from.isValid() ? from : QDateTime::currentDateTime();
    int currentDay = now.date().dayOfWeek() - 1;
    if (currentDay < 0) currentDay = 6;

    bool hasDays = false;
    for (int d = 0; d < 7; ++d) {
        if (a.days[d]) { hasDays = true; break; }
    }

    if (!hasDays) {
        QDateTime dt(now.date(), alarmTime);
        if (dt <= now)
            dt = dt.addDays(1);
        return dt;
    }

    for (int dayOffset = 0; dayOffset < 7; ++dayOffset) {
        int checkDay = (currentDay + dayOffset) % 7;
        if (!a.days[checkDay]) continue;
        QDateTime checkDt(now.date().addDays(dayOffset), alarmTime);
        if (checkDt <= now) continue;
        return checkDt;
    }

    return QDateTime();
}

int Scheduler::nextAlarmSeconds() const
{
    return secondsUntilNextAlarm();
}

QString Scheduler::wakeState() const
{
    return m_wakeState;
}

QString Scheduler::lastError() const
{
    return m_lastError;
}

QString Scheduler::pendingInfo() const
{
    if (!m_pending.valid)
        return QString();
    return tr("RTC wake %1")
        .arg(QDateTime::fromSecsSinceEpoch(m_pending.scheduledUtc)
                 .toLocalTime()
                 .toString("ddd d MMM HH:mm"));
}

int Scheduler::secondsUntilNextAlarm() const
{
    QDateTime now = QDateTime::currentDateTime();
    QTime currentTime = now.time();
    int currentDay = now.date().dayOfWeek() - 1;
    if (currentDay < 0) currentDay = 6;

    int bestDiff = -1;

    const QVector<Alarm> alarms = m_manager->alarmList();
    for (int i = 0; i < alarms.size(); ++i) {
        const Alarm &a = alarms[i];
        if (!a.enabled) continue;

        QTime alarmTime(a.hour, a.minute, 0);

        bool hasDays = false;
        for (int d = 0; d < 7; ++d) {
            if (a.days[d]) { hasDays = true; break; }
        }

        if (!hasDays) {
            QDateTime dt(now.date(), alarmTime);
            if (dt <= now)
                dt = dt.addDays(1);
            int diff = static_cast<int>(now.secsTo(dt));
            if (bestDiff < 0 || diff < bestDiff)
                bestDiff = diff;
            continue;
        }

        for (int dayOffset = 0; dayOffset < 7; ++dayOffset) {
            int checkDay = (currentDay + dayOffset) % 7;
            if (!a.days[checkDay]) continue;

            QDateTime checkDt(now.date().addDays(dayOffset), alarmTime);
            if (checkDt <= now) continue;

            int diff = static_cast<int>(now.secsTo(checkDt));
            if (bestDiff < 0 || diff < bestDiff)
                bestDiff = diff;
            break;
        }
    }

    return bestDiff;
}

void Scheduler::setWakeState(const QString &state, const QString &error)
{
    m_wakeState = state;
    if (!error.isNull())
        m_lastError = error;
    emit wakeStateChanged();
}

bool Scheduler::persistPending()
{
    QString err;
    const bool ok = m_store.persist(m_pending, &err);
    if (!ok) {
        m_lastError = err;
        qWarning() << "[Scheduler] ERROR:" << err;
    }
    return ok;
}

void Scheduler::consumePending(const QString &reason)
{
    if (!m_pending.valid)
        return;
    qInfo() << "[Scheduler] Consuming occurrence" << m_pending.occurrenceId
            << "(" << reason << ")";
    m_pending.state = "consumed";
    if (!persistPending())
        qWarning() << "[Scheduler] ERROR: could not persist consumed state (next boot may re-evaluate)";
    m_wakeState = "idle";
    m_lastError.clear();
    emit wakeStateChanged();
}

bool Scheduler::hasPendingWake() const
{
    return m_pending.valid && m_pending.state != "consumed" && m_pending.state != "failed";
}

void Scheduler::armWake()
{
    if (m_wakeState == "arming" || m_wakeState == "armed") {
        m_lastError = tr("A wake alarm is already armed for this instance");
        emit wakeStateChanged();
        qWarning() << "[Scheduler] armWake refused: already"
                   << (m_wakeState == "armed" ? "armed" : "arming");
        return;
    }

    m_lastError.clear();

    // 1. Calculate the exact next occurrence as an absolute UTC timestamp.
    qint64 best = -1;
    int bestIndex = -1;
    const QVector<Alarm> alarms = m_manager->alarmList();
    for (int i = 0; i < alarms.size(); ++i) {
        const Alarm &a = alarms[i];
        if (!a.enabled) continue;
        const qint64 occ = computeNextOccurrenceUtc(a);
        if (occ <= 0) continue;
        if (best < 0 || occ < best) { best = occ; bestIndex = i; }
    }

    if (bestIndex < 0) {
        m_lastError = tr("No future alarm to arm");
        emit wakeStateChanged();
        qWarning() << "[Scheduler] armWake refused: no future alarm";
        return;
    }

    const Alarm &a = alarms[bestIndex];
    qInfo() << "[Scheduler] Calculated occurrence: alarm" << a.id
            << "local" << QDateTime::fromSecsSinceEpoch(best).toLocalTime().toString(Qt::ISODate);
    qInfo() << "[Scheduler] UTC timestamp:" << best;

    // 2. Persist the occurrence BEFORE touching the RTC. If anything after
    //    this point fails we still must not lose the scheduled wake.
    PendingWake w;
    w.valid = true;
    w.alarmId = a.id;
    w.occurrenceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    w.scheduledUtc = best;
    w.state = "scheduled";
    m_pending = w;
    if (!persistPending()) {
        // Persistence failed → do NOT enter the power-off path.
        m_pending.valid = false;
        setWakeState("failed");
        return;
    }
    qInfo() << "[Scheduler] Persisted occurrence:" << m_pending.occurrenceId
            << "alarm" << m_pending.alarmId << "utc" << m_pending.scheduledUtc;

    // 3. Verify RTC capability.
    const RtcCapabilities caps = m_wake->inspectRtc();
    if (!caps.wakeAlarmSupported) {
        m_pending.state = "failed";
        persistPending();
        setWakeState("failed", caps.error);
        qWarning() << "[Scheduler] ERROR: RTC wake unavailable:" << caps.error;
        return;
    }

    // 4. Program the RTC wake alarm with the absolute timestamp.
    setWakeState("arming");
    QString err;
    if (!m_wake->programWakeAlarm(w.scheduledUtc, &err)) {
        m_pending.state = "failed";
        persistPending();
        setWakeState("failed", err);
        qWarning() << "[Scheduler] ERROR: RTC wake alarm programming failed:" << err;
        return;
    }

    // 5. Verify the alarm was actually accepted by the RTC.
    if (!m_wake->verifyWakeAlarm(w.scheduledUtc, &err)) {
        m_pending.state = "failed";
        persistPending();
        setWakeState("failed", err);
        qWarning() << "[Scheduler] ERROR: RTC wake alarm verification failed:" << err;
        return;
    }

    // 6. Persist final scheduler state ("verified").
    m_pending.state = "verified";
    if (!persistPending()) {
        // Final-state persistence failed: never allow power-off on a
        // half-persisted occurrence.
        m_pending.state = "failed";
        persistPending();
        setWakeState("failed");
        return;
    }

    setWakeState("armed");
    qInfo() << "[Scheduler] RTC wake armed: occurrence" << m_pending.occurrenceId
            << "utc" << m_pending.scheduledUtc;
}

void Scheduler::powerOffMachine()
{
    // Hard safety gate: only an armed + verified + persisted occurrence may
    // power the machine off. Any earlier failure leaves the state "failed"
    // and this refuses.
    if (!m_pending.valid || m_pending.state != "verified" || m_wakeState != "armed") {
        m_lastError = tr("Cannot power off: RTC wake alarm is not armed and verified");
        emit wakeStateChanged();
        qWarning() << "[Scheduler] ERROR: poweroff refused — RTC wake not armed/verified";
        return;
    }

    qInfo() << "[Scheduler] Poweroff requested (occurrence" << m_pending.occurrenceId << ")";
    QString err;
    if (!m_wake->requestPowerOff(&err)) {
        m_lastError = err;
        m_wakeState = "failed";
        emit wakeStateChanged();
        qWarning() << "[Scheduler] ERROR:" << err;
    }
}

void Scheduler::recoverAfterBoot()
{
    if (!m_pending.valid) {
        qInfo() << "[Scheduler] No pending wake occurrence — nothing to recover";
        // Best-effort stale-alarm hygiene: if a wakealarm is still armed but
        // in the past and no occurrence is pending, clear it (likely leftover
        // from a crashed session).
        const qint64 stale = m_wake->readWakeAlarmValue();
        if (stale > 0 && stale < QDateTime::currentSecsSinceEpoch()) {
            qWarning() << "[Scheduler] Stale RTC alarm" << stale << "without pending state; clearing";
            QString err;
            if (!m_wake->clearWakeAlarm(&err))
                qWarning() << "[Scheduler] Could not clear stale RTC alarm:" << err;
        }
        return;
    }

    if (m_pending.state == "consumed") {
        qInfo() << "[Scheduler] Occurrence" << m_pending.occurrenceId
                << "already consumed — no-op (idempotent recovery)";
        return;
    }

    if (m_pending.state == "failed") {
        qInfo() << "[Scheduler] Occurrence" << m_pending.occurrenceId
                << "previously failed to arm — no-op";
        return;
    }

    if (m_pending.state == "scheduled") {
        // The previous session crashed between persistence and RTC
        // programming; the alarm was never armed. Leave it alone — the live
        // 1s ticker will fire it when the time passes.
        qInfo() << "[Scheduler] Occurrence" << m_pending.occurrenceId
                << "never finished arming; left to the live scheduler";
        return;
    }

    const qint64 nowUtc = QDateTime::currentSecsSinceEpoch();

    // Booted before the scheduled time: occurrence is still in the future.
    // Keep it armed; the RTC alarm remains set.
    if (nowUtc + WakePolicy::RTC_ALARM_TOLERANCE_SECONDS < m_pending.scheduledUtc) {
        setWakeState("armed");
        qInfo() << "[Scheduler] Booted before scheduled time; wake remains armed for"
                << QDateTime::fromSecsSinceEpoch(m_pending.scheduledUtc).toUTC().toString(Qt::ISODate) << "UTC";
        return;
    }

    qint64 lateSeconds = 0;
    const WakeResult r = classifyWake(m_pending.scheduledUtc, nowUtc, &lateSeconds);
    const int idx = m_manager->indexOfId(m_pending.alarmId);
    const bool alarmGone = idx < 0 || !m_manager->alarmList()[idx].enabled;

    qInfo() << "[Scheduler] Boot recovery: scheduled"
            << m_pending.scheduledUtc << "now" << nowUtc
            << "→" << wakeResultToString(r) << "late" << lateSeconds << "s";

    if (r == WakeResult::OnTime || r == WakeResult::Late || r == WakeResult::VeryLate) {
        if (alarmGone) {
            qWarning() << "[Scheduler] Alarm" << m_pending.alarmId
                       << "deleted or disabled while scheduled — consuming without triggering";
            consumePending("alarm removed/disabled");
            return;
        }
        // Exactly-once: guard the 1s ticker against re-triggering this
        // occurrence, then trigger, then persist "consumed" so a second
        // evaluation (or a restart) is a no-op.
        m_lastTriggeredTime[m_pending.alarmId] = QDateTime::fromSecsSinceEpoch(nowUtc);
        emit alarmTriggered(idx);
        qInfo() << "[Scheduler] Recovered" << wakeResultToString(r)
                << "wake (late" << lateSeconds << "s) — alarm triggered";
        consumePending("boot recovery");
        return;
    }

    if (r == WakeResult::Missed) {
        qWarning() << "[Scheduler] Missed wake: late" << lateSeconds
                   << "s exceeds RECOVERABLE_WINDOW_SECONDS"
                   << WakePolicy::RECOVERABLE_WINDOW_SECONDS << "— consuming without triggering";
        consumePending("missed");
        return;
    }

    // Unknown: occurrence data invalid or boot before schedule (handled
    // above). Consume deterministically to avoid re-evaluation loops.
    qWarning() << "[Scheduler] Occurrence in unknown state; consuming";
    consumePending("unknown state");
}

void Scheduler::onAlarmsChanged()
{
    // An alarm that was deleted or disabled while its wake is armed cannot be
    // un-programmed from the RTC without privileges, but the machine may
    // still wake. Boot recovery handles that case (alarm gone → consume
    // without triggering). Log the change so it is never silent.
    if (m_pending.valid && m_pending.state == "verified") {
        const int idx = m_manager->indexOfId(m_pending.alarmId);
        if (idx < 0 || !m_manager->alarmList()[idx].enabled) {
            qWarning() << "[Scheduler] Pending wake" << m_pending.occurrenceId
                       << "for alarm" << m_pending.alarmId
                       << "deleted or disabled while scheduled — will not trigger on boot";
        }
    }
}

QString Scheduler::nextAlarmLabel() const
{
    const QDateTime now = QDateTime::currentDateTime();
    QDateTime best;
    QString name;
    const QVector<Alarm> alarms = m_manager->alarmList();
    for (const Alarm &a : alarms) {
        if (!a.enabled)
            continue;
        const QDateTime when = computeNextOccurrence(a, now);
        if (!when.isValid())
            continue;
        if (!best.isValid() || when < best) {
            best = when;
            name = a.name;
        }
    }
    if (!best.isValid())
        return QStringLiteral("Alarm");
    return name.isEmpty()
               ? QStringLiteral("Alarm %1").arg(best.toString(QStringLiteral("HH:mm")))
               : name;
}

void Scheduler::onCheckTimer()
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime prevCheck = m_lastCheckTime;
    m_lastCheckTime = now;

    // Detect system resume from suspend (timer gap > 3s)
    if (prevCheck.isValid()) {
        qint64 gap = prevCheck.msecsTo(now);
        if (gap > 3000) {
            emit systemResumed();
        }
    }

    int secs = secondsUntilNextAlarm();
    emit countdownUpdated(secs);

    // --- Upcoming-alarm notification threshold (STG-5.3) ---
    // Fires once, on the EDGE of the countdown crossing into the 10-minute
    // window. An unknown previous value (first tick after construction, or no
    // alarm scheduled at all -> -1) counts as "outside the window", so an
    // alarm that is already inside it when ticking starts still notifies
    // exactly once instead of being swallowed.
    const int prevCountdown = (m_lastCountdownSecs > 0) ? m_lastCountdownSecs
                                                        : kUpcomingThresholdSeconds + 1;
    if (secs > 0 && secs <= kUpcomingThresholdSeconds
        && prevCountdown > kUpcomingThresholdSeconds) {
        emit alarmApproaching(nextAlarmLabel(), secs / 60);
    }
    m_lastCountdownSecs = secs;

    const QVector<Alarm> alarms = m_manager->alarmList();
    for (int i = 0; i < alarms.size(); ++i) {
        const Alarm &a = alarms[i];
        if (!a.enabled) continue;

        // --- Overdue detection ---
        QDateTime lastTriggered = m_lastTriggeredTime.value(a.id);
        QDateTime refTime = prevCheck.isValid() ? prevCheck : now;
        QDateTime scheduledTime = computeNextOccurrence(a, refTime);

        if (scheduledTime.isValid() && scheduledTime <= now
            && scheduledTime > refTime
            && (!lastTriggered.isValid() || scheduledTime > lastTriggered)) {
            m_lastTriggeredTime[a.id] = now;
            emit alarmTriggered(i);
            if (m_pending.valid && m_pending.state == "verified" && m_pending.alarmId == a.id)
                consumePending("triggered while running");
            continue;
        }

        // --- Exact-time matching (normal tick) ---
        QTime currentTime = now.time();
        if (a.hour == currentTime.hour() && a.minute == currentTime.minute()) {
            QDateTime todayScheduled(now.date(), QTime(a.hour, a.minute, 0));

            bool hasDays = false;
            for (int d = 0; d < 7; ++d) {
                if (a.days[d]) { hasDays = true; break; }
            }
            int currentDay = now.date().dayOfWeek() - 1;
            if (currentDay < 0) currentDay = 6;
            if (hasDays && !a.days[currentDay])
                continue;

            if (!lastTriggered.isValid() || todayScheduled > lastTriggered) {
                m_lastTriggeredTime[a.id] = now;
                emit alarmTriggered(i);
                if (m_pending.valid && m_pending.state == "verified" && m_pending.alarmId == a.id)
                    consumePending("triggered while running");
                continue;
            }
        }
    }

    emit nextAlarmChanged();
}