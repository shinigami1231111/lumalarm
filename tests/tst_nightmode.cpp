#include <QDateTime>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include "managers/AlarmManager.h"
#include "managers/ConfigManager.h"
#include "ui/NightMode.h"
#include "wake/Scheduler.h"
#include "wake/WakeManager.h"

// ============================================================================
// Unit tests for NightMode (src/ui/).
//
// The interesting behaviour is the EDGE trigger: evaluate() runs once per
// scheduler tick, so it must emit activeChanged() exactly once per crossing of
// the lead window — never once per tick. Every case below asserts emission
// COUNTS with QSignalSpy rather than just the final value.
//
// The clock cannot be injected, so the "alarm at 07:00 with minutesBefore=30"
// scenario is exercised by creating an alarm a known number of seconds away and
// moving the lead window across it with setMinutesBefore(). That is exactly the
// same predicate the real tick uses.
// ============================================================================

class TestNightMode : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void defaultsAreDisabledThirtyMinutesAndPointFour();
    void disabledNeverActivates();
    void crossingTheLeadWindowEmitsExactlyOnce();
    void stayingInsideTheWindowDoesNotReemit();
    void leavingTheWindowDeactivatesExactlyOnce();
    void disablingEnabledWhileActiveDeactivatesImmediately();
    void removingTheAlarmWhileActiveDeactivatesImmediately();
    void changingMinutesBeforeReEvaluates();
    void noAlarmScheduledNeverActivates();
    void previewStaysOnForTwoSecondsDespiteTicks();
    void settingsRoundTripAndClamp();

private:
    // An enabled alarm `seconds` from now, on every weekday.
    void scheduleAlarmIn(int seconds);

    AlarmManager *m_alarms = nullptr;
    WakeManager *m_wake = nullptr;
    Scheduler *m_scheduler = nullptr;
    ConfigManager *m_config = nullptr;
    NightMode *m_night = nullptr;
};

void TestNightMode::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    m_alarms = new AlarmManager(this);
    m_wake = new WakeManager(this);
    m_scheduler = new Scheduler(m_alarms, m_wake, QString(), this);
    m_config = new ConfigManager(this);
    m_night = new NightMode(m_config, m_scheduler, this);
}

void TestNightMode::init()
{
    // Deterministic starting point for every case.
    m_alarms->setAlarms({});
    m_config->setNightModeEnabled(false);
    m_config->setNightModeMinutesBefore(30);
    m_config->setNightModeOpacity(0.4);
    m_night->deactivate();
}

void TestNightMode::scheduleAlarmIn(int seconds)
{
    Alarm a;
    const QDateTime t = QDateTime::currentDateTime().addSecs(seconds);
    a.hour = t.time().hour();
    a.minute = t.time().minute();
    for (int d = 0; d < 7; ++d)
        a.days[d] = true;
    a.enabled = true;
    m_alarms->setAlarms({a});
}

void TestNightMode::defaultsAreDisabledThirtyMinutesAndPointFour()
{
    // A fresh ConfigManager (no keys written yet) must report the documented
    // defaults. init() has written explicit values, so clear them first.
    const QString path = m_config->configFilePath();
    QFile::remove(path);
    ConfigManager fresh;

    QCOMPARE(fresh.nightModeEnabled(), false);
    QCOMPARE(fresh.nightModeMinutesBefore(), 30);
    QCOMPARE(fresh.nightModeOpacity(), 0.4);
    QVERIFY(path.endsWith("settings.ini"));
}

void TestNightMode::disabledNeverActivates()
{
    // Alarm 10 s away, lead window 30 min → deep inside it, but the feature is
    // off, so evaluate() must not activate.
    scheduleAlarmIn(600);
    m_config->setNightModeMinutesBefore(30);
    m_night->deactivate();

    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_night->evaluate();
    m_night->evaluate();
    m_night->evaluate();

    QCOMPARE(m_night->active(), false);
    QCOMPARE(spy.count(), 0);
}

void TestNightMode::crossingTheLeadWindowEmitsExactlyOnce()
{
    // 30 minutes ahead with a 30 minute lead window == exactly on the boundary.
    scheduleAlarmIn(30 * 60);
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(30);
    m_night->deactivate();

    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_night->evaluate();

    // Crossing into the window: exactly one emission, carrying true.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(m_night->active(), true);
}

void TestNightMode::stayingInsideTheWindowDoesNotReemit()
{
    scheduleAlarmIn(30 * 60);
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(30);
    m_night->deactivate();
    m_night->evaluate();
    QVERIFY(m_night->active());

    // Ten more ticks inside the same window (the 1 Hz scheduler path).
    QSignalSpy spy(m_night, &NightMode::activeChanged);
    for (int i = 0; i < 10; ++i)
        m_night->evaluate();

    QCOMPARE(spy.count(), 0); // no duplicates
    QCOMPARE(m_night->active(), true);
}

void TestNightMode::leavingTheWindowDeactivatesExactlyOnce()
{
    scheduleAlarmIn(30 * 60);
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(30);
    m_night->deactivate();
    m_night->evaluate();
    QVERIFY(m_night->active());

    // Shrink the lead window: the next alarm is no longer inside it.
    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_config->setNightModeMinutesBefore(1);
    m_night->evaluate();
    m_night->evaluate();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
    QCOMPARE(m_night->active(), false);
}

void TestNightMode::disablingEnabledWhileActiveDeactivatesImmediately()
{
    scheduleAlarmIn(120);
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(30);
    m_night->evaluate();
    QVERIFY(m_night->active());

    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_night->setEnabled(false); // no evaluate() call: must be immediate

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_night->active(), false);
}

void TestNightMode::removingTheAlarmWhileActiveDeactivatesImmediately()
{
    scheduleAlarmIn(120);
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(30);
    m_night->evaluate();
    QVERIFY(m_night->active());

    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_alarms->setAlarms({}); // alarm deleted / disabled
    m_night->evaluate();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_night->active(), false);
}

void TestNightMode::changingMinutesBeforeReEvaluates()
{
    scheduleAlarmIn(600); // 10 minutes away
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(1); // window is 60 s → not inside yet
    m_night->deactivate();
    m_night->evaluate();
    QCOMPARE(m_night->active(), false);

    // Widening the window past the alarm activates without an explicit tick.
    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_night->setMinutesBefore(30);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_night->active(), true);

    // And narrowing it again deactivates, also without an explicit tick.
    m_night->setMinutesBefore(1);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(m_night->active(), false);
}

void TestNightMode::noAlarmScheduledNeverActivates()
{
    // No alarms at all: evaluate() must be a pure no-op even when enabled.
    m_alarms->setAlarms({});
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(180); // widest possible window
    m_night->deactivate();

    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_night->evaluate();
    m_night->evaluate();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(m_night->active(), false);
}

void TestNightMode::previewStaysOnForTwoSecondsDespiteTicks()
{
    // An alarm far outside any window, so the 1 Hz tick would switch the veil
    // off immediately — unless the manual preview owns it.
    scheduleAlarmIn(3600);
    m_config->setNightModeEnabled(true);
    m_config->setNightModeMinutesBefore(1);
    m_night->deactivate();

    QSignalSpy spy(m_night, &NightMode::activeChanged);
    m_night->activate();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_night->active(), true);

    // Tick twice inside the preview window: still on, still no extra emission.
    m_night->evaluate();
    m_night->evaluate();
    QCOMPARE(m_night->active(), true);
    QCOMPARE(spy.count(), 1);

    // ...and it ends by itself after the documented two seconds.
    QTRY_COMPARE_WITH_TIMEOUT(m_night->active(), false, NightMode::kPreviewMillis + 2000);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void TestNightMode::settingsRoundTripAndClamp()
{
    m_config->setNightModeEnabled(true);
    QCOMPARE(m_config->nightModeEnabled(), true);
    m_config->setNightModeEnabled(false);
    QCOMPARE(m_config->nightModeEnabled(), false);

    m_config->setNightModeMinutesBefore(45);
    QCOMPARE(m_config->nightModeMinutesBefore(), 45);
    m_config->setNightModeMinutesBefore(0); // below range → clamped to 1
    QCOMPARE(m_config->nightModeMinutesBefore(), 1);
    m_config->setNightModeMinutesBefore(9999); // above range → clamped to 180
    QCOMPARE(m_config->nightModeMinutesBefore(), 180);

    m_config->setNightModeOpacity(0.75);
    QCOMPARE(m_config->nightModeOpacity(), 0.75);
    m_config->setNightModeOpacity(5.0); // above range → clamped to 1.0
    QCOMPARE(m_config->nightModeOpacity(), 1.0);
    m_config->setNightModeOpacity(-1.0); // below range → clamped to 0.0
    QCOMPARE(m_config->nightModeOpacity(), 0.0);

    // Round-trip through a fresh instance over the same file.
    m_config->setNightModeMinutesBefore(20);
    m_config->setNightModeOpacity(0.55);
    ConfigManager reloaded;
    QCOMPARE(reloaded.nightModeMinutesBefore(), 20);
    QCOMPARE(reloaded.nightModeOpacity(), 0.55);

    // Restore sane values for any later case.
    m_config->setNightModeMinutesBefore(30);
    m_config->setNightModeOpacity(0.4);
}

QTEST_GUILESS_MAIN(TestNightMode)
#include "tst_nightmode.moc"
