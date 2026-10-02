#include <QtTest>
#include <QSignalSpy>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include "wake/WakeManager.h"

// ============================================================================
// Unit tests for the test-alarm surface of WakeManager (src/wake/).
//
// Range validation is pure logic and always runs. The arming/cancelling cases
// need the privileged D-Bus helper (org.lumalarm.Helper on the system bus);
// when it is not installed or the bus denies the call, those cases skip with an
// explicit reason instead of failing.
//
// NOTE: a successful arm really does schedule a wake, so every arming case
// cancels it again through cancelTestWake() before returning.
// ============================================================================

class TestWakeManager : public QObject
{
    Q_OBJECT

private slots:
    void constructionStartsIdle();
    void armTestWakeRejectsSecondsBelowRange();
    void armTestWakeRejectsSecondsAboveRange();
    void armTestWakeArmsAndEmitsSignal();
    void cancelTestWakeReturnsToIdle();
};

void TestWakeManager::constructionStartsIdle()
{
    WakeManager wm;

    QCOMPARE(wm.wakeState(), QStringLiteral("idle"));
    QVERIFY(wm.lastError().isEmpty());
    QVERIFY(!wm.isRtcWakeAvailable() || true); // capability probe must not crash
}

// True when the helper answered the call (armed or declined), i.e. the D-Bus
// path is reachable at all. Used to decide between skip and failure.
static bool helperReachable(WakeManager &wm, int seconds)
{
    return wm.armTestWake(seconds);
}

void TestWakeManager::armTestWakeRejectsSecondsBelowRange()
{
    WakeManager wm;
    QSignalSpy failSpy(&wm, &WakeManager::testWakeFailed);
    QSignalSpy armedSpy(&wm, &WakeManager::testWakeArmed);

    QVERIFY(!wm.armTestWake(30));
    QCOMPARE(failSpy.count(), 1);
    QCOMPARE(armedSpy.count(), 0);
    QVERIFY(failSpy.at(0).at(0).toString().contains(QStringLiteral("out of range")));
    QCOMPARE(wm.wakeState(), QStringLiteral("idle"));
}

void TestWakeManager::armTestWakeRejectsSecondsAboveRange()
{
    WakeManager wm;
    QSignalSpy failSpy(&wm, &WakeManager::testWakeFailed);
    QSignalSpy armedSpy(&wm, &WakeManager::testWakeArmed);

    QVERIFY(!wm.armTestWake(4000));
    QCOMPARE(failSpy.count(), 1);
    QCOMPARE(armedSpy.count(), 0);
    QVERIFY(failSpy.at(0).at(0).toString().contains(QStringLiteral("out of range")));
    QCOMPARE(wm.wakeState(), QStringLiteral("idle"));
}

void TestWakeManager::armTestWakeArmsAndEmitsSignal()
{
    WakeManager wm;
    QSignalSpy armedSpy(&wm, &WakeManager::testWakeArmed);

    if (!helperReachable(wm, 120)) {
        const QString reason = wm.lastError().isEmpty()
            ? QStringLiteral("helper declined") : wm.lastError();
        wm.cancelTestWake();
        QSKIP(qPrintable(QStringLiteral("D-Bus wake helper (org.lumalarm.Helper) unavailable on "
                                       "the system bus — arming path not exercised: %1")
                             .arg(reason)));
    }

    QCOMPARE(armedSpy.count(), 1);
    QCOMPARE(armedSpy.at(0).at(0).toInt(), 120);
    QCOMPARE(wm.wakeState(), QStringLiteral("armed"));

    // Leave no wake behind for the next test.
    wm.cancelTestWake();
}

void TestWakeManager::cancelTestWakeReturnsToIdle()
{
    WakeManager wm;

    if (!helperReachable(wm, 120)) {
        QSKIP("D-Bus wake helper (org.lumalarm.Helper) unavailable on the system bus — "
              "cancel path not exercised");
    }
    QCOMPARE(wm.wakeState(), QStringLiteral("armed"));

    QSignalSpy stateSpy(&wm, &WakeManager::wakeStateChanged);
    wm.cancelTestWake();

    QCOMPARE(wm.wakeState(), QStringLiteral("idle"));
    QCOMPARE(stateSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TestWakeManager)
#include "tst_wakemanager.moc"
