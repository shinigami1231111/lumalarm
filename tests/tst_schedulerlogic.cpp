#include <QtTest>

#include "managers/AlarmManager.h"
#include "wake/Scheduler.h"
#include "wake/SchedulerState.h"
#include "wake/WakeManager.h"

// ============================================================================
// Unit tests for the scheduler wake-recovery logic:
//  - occurrence calculation as absolute UTC timestamps (incl. DST)
//  - boot classification (on-time / late / very-late / missed)
//  - exactly-once triggering across restarts (idempotent recovery)
//  - persistence failure → power-off refused
//  - clock-change determinism
// ============================================================================

class TestSchedulerLogic : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void classificationNearPerfectWake();
    void classificationLateWake();
    void classificationLongBootDelay();
    void classificationMissedWindow();
    void occurrenceIsAbsoluteUtc();
    void occurrenceOnceThenNextDay();
    void occurrenceDstSpringForward();
    void occurrenceDstFallBack();
    void clockChangeDeterministic();
    void persistenceRoundTrip();
    void persistenceFailureRefusesPoweroff();
    void duplicateRecoveryIsNoOp();
    void rtcFailureRefusesPoweroff();
    void poweroffRefusesWhenScheduledWakeIsGone();
    void armedStateDropsWhenScheduledWakeDisappears();

private:
    static Alarm makeAlarm(const QString &id, int hour, int minute);
    QString storePath(const char *name) const;
};

void TestSchedulerLogic::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    // Isolate each test's store file.
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

Alarm TestSchedulerLogic::makeAlarm(const QString &id, int hour, int minute)
{
    Alarm a;
    a.id = id;
    a.hour = hour;
    a.minute = minute;
    a.enabled = true;
    return a;
}

QString TestSchedulerLogic::storePath(const char *name) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" + name +
           ".json";
}

void TestSchedulerLogic::classificationNearPerfectWake()
{
    // scheduled 07:00:00, boot 07:00:02 → expected wake, ~on time
    const qint64 scheduled = 1700000000;
    qint64 late = -1;
    QCOMPARE(classifyWake(scheduled, scheduled + 2, &late), WakeResult::OnTime);
    QCOMPARE(late, 2);
}

void TestSchedulerLogic::classificationLateWake()
{
    // scheduled 07:00:00, boot 07:01:00 → late but within grace → trigger
    const qint64 scheduled = 1700000000;
    qint64 late = -1;
    QCOMPARE(classifyWake(scheduled, scheduled + 60, &late), WakeResult::Late);
    QCOMPARE(late, 60);
}

void TestSchedulerLogic::classificationLongBootDelay()
{
    // scheduled 07:00:00, boot 08:00:00 → very late, still recoverable → trigger
    const qint64 scheduled = 1700000000;
    qint64 late = -1;
    QCOMPARE(classifyWake(scheduled, scheduled + 3600, &late), WakeResult::VeryLate);
    QCOMPARE(late, 3600);
}

void TestSchedulerLogic::classificationMissedWindow()
{
    // scheduled 07:00:00, boot 3 days later → missed → no trigger
    const qint64 scheduled = 1700000000;
    QCOMPARE(classifyWake(scheduled, scheduled + 3 * 86400), WakeResult::Missed);
    // invalid data → unknown
    QCOMPARE(classifyWake(0, 1700000000), WakeResult::Unknown);
    // boot before scheduled time → unknown (still pending)
    QCOMPARE(classifyWake(scheduled, scheduled - 3600), WakeResult::Unknown);
}

void TestSchedulerLogic::occurrenceIsAbsoluteUtc()
{
    // Use an explicit zone for both the calculation and the round-trip. The
    // old version mixed QTimeZone::systemTimeZone() (used by the default
    // argument) with QDateTime::toLocalTime(); those can resolve to different
    // zones inside a minimal CI container, so the epoch → wall-clock check
    // failed on Arch while the identical binary passed on Ubuntu/Fedora. This
    // test is about absolute UTC arithmetic, not the host's /etc/localtime.
    const QTimeZone fixedUtc("UTC");
    QVERIFY2(fixedUtc.isValid(), "UTC timezone data unavailable");

    const QDateTime now = QDateTime(QDate(2026, 7, 15), QTime(10, 30, 0), fixedUtc);
    const Alarm a = makeAlarm("utc1", 7, 30);
    const qint64 occ = computeNextOccurrenceUtc(a, now, fixedUtc);
    QVERIFY(occ > now.toSecsSinceEpoch());

    // Round-trip: UTC epoch → 07:30 next day (today's 07:30 already passed).
    const QDateTime wall = QDateTime::fromSecsSinceEpoch(occ).toTimeZone(fixedUtc);
    QCOMPARE(wall.date(), QDate(2026, 7, 16));
    QCOMPARE(wall.time().hour(), 7);
    QCOMPARE(wall.time().minute(), 30);
}

void TestSchedulerLogic::occurrenceOnceThenNextDay()
{
    const QDateTime now = QDateTime(QDate(2026, 7, 15), QTime(7, 30, 0));
    const Alarm a = makeAlarm("once1", 7, 30);
    // Exactly at the scheduled time: next occurrence must be tomorrow.
    const qint64 occ = computeNextOccurrenceUtc(a, now);
    const QDate day = QDateTime::fromSecsSinceEpoch(occ).toLocalTime().date();
    QCOMPARE(day, QDate(2026, 7, 16));
}

void TestSchedulerLogic::occurrenceDstSpringForward()
{
    const QTimeZone ny("America/New_York");
    if (!ny.isValid())
        QSKIP("America/New_York timezone data unavailable");

    // US spring forward: 2026-03-08 02:00 → 03:00 EDT. A 02:30 alarm does not
    // exist that day; it must be mapped to 03:30 EDT (= 07:30 UTC).
    const QDateTime now = QDateTime(QDate(2026, 3, 7), QTime(12, 0, 0), ny);
    const Alarm a = makeAlarm("dst-fwd", 2, 30);
    const qint64 occ = computeNextOccurrenceUtc(a, now, ny);

    const qint64 expected =
        QDateTime(QDate(2026, 3, 8), QTime(3, 30, 0), ny).toUTC().toSecsSinceEpoch();
    QCOMPARE(occ, expected);

    // A 01:30 alarm on the same day must be unaffected (before the jump).
    const Alarm b = makeAlarm("dst-fwd2", 1, 30);
    const qint64 occ2 = computeNextOccurrenceUtc(b, now, ny);
    QCOMPARE(occ2, QDateTime(QDate(2026, 3, 8), QTime(1, 30, 0), ny).toUTC().toSecsSinceEpoch());
}

void TestSchedulerLogic::occurrenceDstFallBack()
{
    const QTimeZone ny("America/New_York");
    if (!ny.isValid())
        QSKIP("America/New_York timezone data unavailable");

    // US fall back: 2026-11-01 02:00 → 01:00 EST. 01:30 occurs twice (EDT and
    // EST). The result must be deterministic: computing twice yields the same
    // absolute instant.
    const QDateTime now = QDateTime(QDate(2026, 10, 31), QTime(12, 0, 0), ny);
    const Alarm a = makeAlarm("dst-back", 1, 30);
    const qint64 occ1 = computeNextOccurrenceUtc(a, now, ny);
    const qint64 occ2 = computeNextOccurrenceUtc(a, now, ny);
    QCOMPARE(occ1, occ2);

    // And it must be one of the two real instants that 01:30 maps to.
    const qint64 edt =
        QDateTime(QDate(2026, 11, 1), QTime(1, 30, 0), QTimeZone("UTC-04:00")).toSecsSinceEpoch();
    const qint64 est =
        QDateTime(QDate(2026, 11, 1), QTime(1, 30, 0), QTimeZone("UTC-05:00")).toSecsSinceEpoch();
    QVERIFY(occ1 == edt || occ1 == est);
}

void TestSchedulerLogic::clockChangeDeterministic()
{
    // An occurrence is scheduled as an absolute timestamp. A manual clock
    // change after scheduling must not alter the scheduled instant — only the
    // classification of the resulting wake changes, deterministically.
    const qint64 scheduled = 1700000000;
    // Clock moved FORWARD by 4 hours while off: very late wake → trigger.
    QCOMPARE(classifyWake(scheduled, scheduled + 4 * 3600), WakeResult::VeryLate);
    // Clock moved BACKWARD by 1 hour while off: boot lands 1h early → pending.
    QCOMPARE(classifyWake(scheduled, scheduled - 3600), WakeResult::Unknown);
    // Clock moved forward 2 days: missed → no trigger.
    QCOMPARE(classifyWake(scheduled, scheduled + 2 * 86400), WakeResult::Missed);
}

void TestSchedulerLogic::persistenceRoundTrip()
{
    const QString path = storePath("roundtrip");
    QFile::remove(path);
    QFile::remove(path + ".bak");

    PendingWake w;
    w.valid = true;
    w.alarmId = "alarm-1";
    w.occurrenceId = "occ-1";
    w.scheduledUtc = 1700000000;
    w.state = "verified";

    {
        SchedulerStateStore store(path);
        QString err;
        QVERIFY2(store.persist(w, &err), qPrintable(err));
    }
    {
        SchedulerStateStore store(path);
        const PendingWake &r = store.pending();
        QVERIFY(r.valid);
        QCOMPARE(r.alarmId, "alarm-1");
        QCOMPARE(r.occurrenceId, "occ-1");
        QCOMPARE(r.scheduledUtc, 1700000000);
        QCOMPARE(r.state, "verified");
    }
    QFile::remove(path);
}

void TestSchedulerLogic::persistenceFailureRefusesPoweroff()
{
    // Persistence into an unwritable location must fail loudly and leave the
    // scheduler unable to power off (the gate requires "armed"/verified state,
    // which is only reachable after a successful persist).
    const QString path = "/proc/lumalarm-impossible/scheduler.json"; // unwritable
    SchedulerStateStore store(path);
    PendingWake w;
    w.valid = true;
    w.alarmId = "a";
    w.occurrenceId = "o";
    w.scheduledUtc = 1700000000;
    w.state = "scheduled";
    QString err;
    QVERIFY(!store.persist(w, &err));
    QVERIFY(!err.isEmpty());

    // The store must still report "nothing persisted" — no half-written state.
    QVERIFY(!store.pending().valid);
}

void TestSchedulerLogic::duplicateRecoveryIsNoOp()
{
    // Scenario: occurrence armed before poweroff (state verified), machine
    // boots 2 minutes late. First Scheduler instance recovers → triggers once
    // and persists "consumed". A second instance over the same state must be
    // a no-op.
    const QString path = storePath("duprec");
    QFile::remove(path);
    QFile::remove(path + ".bak");

    AlarmManager am;
    const Alarm a = makeAlarm("dup-alarm", 7, 0);
    am.setAlarms(QVector<Alarm>() << a);

    PendingWake w;
    w.valid = true;
    w.alarmId = a.id;
    w.occurrenceId = "occ-dup";
    w.scheduledUtc = QDateTime::currentSecsSinceEpoch() - 120; // boot 2 min late
    w.state = "verified";
    {
        SchedulerStateStore seed(path);
        QVERIFY(seed.persist(w));
    }

    WakeManager wm;
    {
        Scheduler s1(&am, &wm, path);
        QSignalSpy spy(&s1, &Scheduler::alarmTriggered);
        QVERIFY(spy.wait(2000)); // recovered + triggered exactly once
        QCOMPARE(spy.count(), 1);
        QCOMPARE(s1.wakeState(), QString("idle"));
    }
    {
        Scheduler s2(&am, &wm, path);
        QSignalSpy spy(&s2, &Scheduler::alarmTriggered);
        QTest::qWait(300);        // give the deferred recovery a chance to run
        QCOMPARE(spy.count(), 0); // consumed → no-op
    }
    QFile::remove(path);
}

void TestSchedulerLogic::rtcFailureRefusesPoweroff()
{
    // An occurrence that failed to arm (state "failed", e.g. RTC programming
    // failure) must never allow power-off. powerOffMachine() is a hard gate.
    const QString path = storePath("rtcfail");
    QFile::remove(path);
    QFile::remove(path + ".bak");

    AlarmManager am;
    const Alarm a = makeAlarm("rtc-alarm", 7, 0);
    am.setAlarms(QVector<Alarm>() << a);

    PendingWake w;
    w.valid = true;
    w.alarmId = a.id;
    w.occurrenceId = "occ-rtc";
    w.scheduledUtc = QDateTime::currentSecsSinceEpoch() - 120;
    w.state = "failed";
    {
        SchedulerStateStore seed(path);
        QVERIFY(seed.persist(w));
    }

    WakeManager wm;
    Scheduler s(&am, &wm, path);
    QTest::qWait(300);   // let the deferred recovery run (state "failed" → no-op)
    s.powerOffMachine(); // must refuse: state is failed, never armed
    QCOMPARE(s.wakeState(), QString("idle"));
    QVERIFY(!s.lastError().isEmpty());
    QVERIFY(s.lastError().contains("not armed"));
}

// A persisted "verified" occurrence records what was true at arm time. The wake
// underneath can be gone by the time the user acts — Settings' "Arm test alarm"
// replaces it, "Cancel test alarm" removes it, a test wake expires, a reboot
// drops the transient systemd timer — and nothing here scheduled a wake at all.
void TestSchedulerLogic::poweroffRefusesWhenScheduledWakeIsGone()
{
    const QString path = storePath("poweroffgone");
    QFile::remove(path);
    QFile::remove(path + ".bak");

    AlarmManager am;
    const QTime inThreeHours = QTime::currentTime().addSecs(3 * 3600);
    const Alarm a = makeAlarm("poweroff-gone", inThreeHours.hour(), inThreeHours.minute());
    am.setAlarms(QVector<Alarm>() << a);

    PendingWake w;
    w.valid = true;
    w.alarmId = a.id;
    w.occurrenceId = "occ-poweroff-gone";
    w.scheduledUtc = QDateTime::currentSecsSinceEpoch() + 3 * 3600;
    w.state = "verified";
    {
        SchedulerStateStore seed(path);
        QVERIFY(seed.persist(w));
    }

    WakeManager wm;
    Scheduler s(&am, &wm, path);
    QTest::qWait(300); // deferred recovery -> "armed"
    QCOMPARE(s.wakeState(), QString("armed"));

    s.powerOffMachine(); // live re-check finds no wake in place
    QCOMPARE(s.wakeState(), QString("failed"));
    QVERIFY2(s.lastError().contains("no longer in place"), qPrintable(s.lastError()));
    QVERIFY(s.lastError().contains("Cannot power off"));
    QFile::remove(path);
}

// Same starting state, but no one presses anything: the ticker itself has to
// notice the advertised wake is gone, instead of leaving "Armed" — and the
// enabled Power Off & Wake button — up indefinitely.
void TestSchedulerLogic::armedStateDropsWhenScheduledWakeDisappears()
{
    const QString path = storePath("armeddrops");
    QFile::remove(path);
    QFile::remove(path + ".bak");

    AlarmManager am;
    const QTime inThreeHours = QTime::currentTime().addSecs(3 * 3600);
    const Alarm a = makeAlarm("armed-drops", inThreeHours.hour(), inThreeHours.minute());
    am.setAlarms(QVector<Alarm>() << a);

    PendingWake w;
    w.valid = true;
    w.alarmId = a.id;
    w.occurrenceId = "occ-armed-drops";
    w.scheduledUtc = QDateTime::currentSecsSinceEpoch() + 3 * 3600;
    w.state = "verified";
    {
        SchedulerStateStore seed(path);
        QVERIFY(seed.persist(w));
    }

    WakeManager wm;
    Scheduler s(&am, &wm, path);
    QTest::qWait(300);
    QCOMPARE(s.wakeState(), QString("armed"));

    // First tick lands ~1 s after construction, so allow the interval plus slack.
    QTRY_COMPARE_WITH_TIMEOUT(s.wakeState(), QString("failed"),
                              1000 * (Scheduler::kWakeReverifyIntervalTicks + 5));
    QVERIFY2(s.lastError().contains("no longer in place"), qPrintable(s.lastError()));
    QFile::remove(path);
}

QTEST_GUILESS_MAIN(TestSchedulerLogic)
#include "tst_schedulerlogic.moc"