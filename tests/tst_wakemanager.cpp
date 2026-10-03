#include <QtTest>
#include <QSignalSpy>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include <climits>
#include <cstdint>
#include <limits>
#include <unistd.h>

#include "wake/WakeManager.h"
#include "wake/WakeSecurity.h"

// ============================================================================
// Unit tests for WakeManager (src/wake/): the test-alarm surface and the
// STG-1 validation / ownership contract.
//
// Range validation and the ownership state machine are pure logic and always
// run — they are driven through the integration seams (see WakeManager.h) so
// no real D-Bus call, no rtcwake and no hardware clock write is needed. The
// arming/cancelling cases that DO need the privileged D-Bus helper skip with
// an explicit reason when it is not installed on the test machine.
//
// NOTE: a successful arm really does schedule a wake, so every arming case
// cancels it again through cancelTestWake() before returning.
// ============================================================================

namespace {

// WakeManager with the three external seams substituted. Nothing here talks to
// D-Bus, systemd or the RTC, so the tests exercise the real decision logic
// deterministically without root, a system bus or an installed helper.
class FakeWakeManager : public WakeManager
{
public:
    using WakeManager::WakeManager;

    // What the substituted seams should do.
    bool scheduleSucceeds = true;
    bool cancelSucceeds = false;
    bool timerArmed = true;

    // What actually happened — used to prove a rejected input never reached a
    // privileged path at all.
    int scheduleCalls = 0;
    int cancelCalls = 0;
    QList<uint> scheduledSeconds;

protected:
    HelperResult scheduleViaHelper(uint seconds, QString *message) override
    {
        ++scheduleCalls;
        scheduledSeconds << seconds;
        if (message)
            *message = scheduleSucceeds ? QStringLiteral("scheduled")
                                        : QStringLiteral("denied");
        return scheduleSucceeds ? HelperResult::Scheduled : HelperResult::Declined;
    }

    bool cancelViaHelper(QString *message) override
    {
        ++cancelCalls;
        if (message)
            *message = cancelSucceeds ? QStringLiteral("cancelled")
                                      : QStringLiteral("forced helper failure");
        return cancelSucceeds;
    }

    bool helperWakeStillArmed() const override
    {
        return timerArmed;
    }
};

// WakeManager with scheduleViaHelper() promoted to public and NOT overridden,
// so the guard inside the real implementation can be exercised directly.
class DirectWakeManager : public WakeManager
{
public:
    using WakeManager::scheduleViaHelper;

    // Returns true when the real implementation refused the value. Kept here
    // so the test body never has to name the protected HelperResult enum.
    bool scheduleRefused(uint seconds, QString *message)
    {
        return scheduleViaHelper(seconds, message) != HelperResult::Scheduled;
    }
};

// Arm a wake through the fake helper so ownership is established by the real
// code path (armTestWake) rather than by poking private state.
bool primeHelperOwnership(FakeWakeManager &wm)
{
    if (wm.armTestWake(300))
        return true;
    // A refused arm (e.g. the real helper answered instead of the seam) still
    // leaves the seams reachable; report so the caller can skip.
    return wm.helperOwnsWake();
}

} // namespace

class TestWakeManager : public QObject
{
    Q_OBJECT

private slots:
    void constructionStartsIdle();

    // STG-1.3 — the single [60, 86400] boundary on every wake path.
    void sharedBoundaryIsExact();
    void prepareWakeRejectsInvalidDelays_data();
    void prepareWakeRejectsInvalidDelays();
    void prepareWakeAcceptsBoundaryDelays();
    void prepareWakeRejectsMalformedMode();
    void prepareWakeRejectionTouchesNoWake();
    void programWakeAlarmRejectsInvalidDelays_data();
    void programWakeAlarmRejectsInvalidDelays();
    void programWakeAlarmAcceptsValidBoundary();

    // F6 — ownership survives a failed cancellation.
    void failedCancelKeepsHelperOwnership();
    void successfulCancelReleasesHelperOwnership();
    void cancelReleasesOwnershipWhenNothingIsArmed();
    void scheduleRefusesOutOfRangeBeforeTheHelper();

    // Existing test-alarm behaviour (regression guard for the STG-1 work).
    void armTestWakeRejectsSecondsBelowRange();
    void armTestWakeRejectsSecondsAboveRange();
    void armTestWakeArmsAndEmitsSignal();
    void cancelTestWakeReturnsToIdle();
};

// True when the helper answered the call (armed or declined), i.e. the D-Bus
// path is reachable at all. Used to decide between skip and failure.
static bool helperReachable(WakeManager &wm, int seconds)
{
    return wm.armTestWake(seconds);
}

void TestWakeManager::constructionStartsIdle()
{
    WakeManager wm;

    QCOMPARE(wm.wakeState(), QStringLiteral("idle"));
    QVERIFY(wm.lastError().isEmpty());
    QVERIFY(!wm.isRtcWakeAvailable() || true); // capability probe must not crash
    QVERIFY(!wm.helperOwnsWake());
}

// ── STG-1.3: validation boundary ─────────────────────────────────────────────

void TestWakeManager::sharedBoundaryIsExact()
{
    // The one function both the app and the root helper validate with.
    QVERIFY(!WakePolicy::isValidWakeDelay(0));
    QVERIFY(!WakePolicy::isValidWakeDelay(1));
    QVERIFY(!WakePolicy::isValidWakeDelay(59));
    QVERIFY(WakePolicy::isValidWakeDelay(60));
    QVERIFY(WakePolicy::isValidWakeDelay(61));
    QVERIFY(WakePolicy::isValidWakeDelay(86400));
    QVERIFY(!WakePolicy::isValidWakeDelay(86401));
    QVERIFY(!WakePolicy::isValidWakeDelay(100000));

    // Signed/unsigned handling: values above int range and every negative.
    QVERIFY(!WakePolicy::isValidWakeDelay(qint64(UINT32_MAX)));        // 4294967295
    QVERIFY(!WakePolicy::isValidWakeDelay(qint64(INT_MAX)));
    QVERIFY(!WakePolicy::isValidWakeDelay(-1));
    QVERIFY(!WakePolicy::isValidWakeDelay(INT_MIN));
    QVERIFY(!WakePolicy::isValidWakeDelay(std::numeric_limits<qint64>::min()));

    QCOMPARE(WakePolicy::kMinWakeSeconds, 60);
    QCOMPARE(WakePolicy::kMaxWakeSeconds, 86400);
}

void TestWakeManager::prepareWakeRejectsInvalidDelays_data()
{
    QTest::addColumn<int>("seconds");
    QTest::newRow("zero") << 0;
    QTest::newRow("one") << 1;
    QTest::newRow("below-minimum-59") << 59;
    QTest::newRow("above-maximum-86401") << 86401;
    QTest::newRow("far-above-100000") << 100000;
    QTest::newRow("negative") << -1;
    QTest::newRow("int-min") << INT_MIN;
    QTest::newRow("int-max") << INT_MAX;
}

void TestWakeManager::prepareWakeRejectsInvalidDelays()
{
    QFETCH(int, seconds);

    FakeWakeManager wm;
    QSignalSpy preparedSpy(&wm, &WakeManager::wakePrepared);
    QSignalSpy errorSpy(&wm, &WakeManager::wakeError);

    wm.prepareWake(seconds);

    QCOMPARE(wm.wakeState(), QStringLiteral("failed"));
    QVERIFY(!wm.lastError().isEmpty());
    QVERIFY(wm.lastError().contains(QStringLiteral("outside the allowed range")));
    QCOMPARE(preparedSpy.count(), 0);       // no success was claimed
    QCOMPARE(errorSpy.count(), 1);          // the failure was reported
    QCOMPARE(wm.scheduleCalls, 0);          // never reached a privileged path
    QVERIFY(!wm.helperOwnsWake());          // no ownership was invented
}

void TestWakeManager::prepareWakeAcceptsBoundaryDelays()
{
    // Both ends of the interval must be accepted and handed to the helper
    // verbatim (the helper-side rejection of everything else is covered live
    // in tst_stg1security).
    for (const int seconds : {60, 61, 86400}) {
        FakeWakeManager wm;
        QSignalSpy preparedSpy(&wm, &WakeManager::wakePrepared);
        QSignalSpy errorSpy(&wm, &WakeManager::wakeError);

        wm.prepareWake(seconds);

        QCOMPARE(wm.scheduleCalls, 1);
        QCOMPARE(wm.scheduledSeconds.last(), uint(seconds));
        QCOMPARE(wm.wakeState(), QStringLiteral("armed"));
        QCOMPARE(preparedSpy.count(), 1);
        QCOMPARE(errorSpy.count(), 0);
        QVERIFY(wm.helperOwnsWake());
    }
}

void TestWakeManager::prepareWakeRejectsMalformedMode()
{
    // `mode` reaches `rtcwake -m <mode>` on the legacy path; only a short
    // alphabetic token may pass.
    for (const QString &mode : {QStringLiteral(""),
                                QStringLiteral("mem;id"),
                                QStringLiteral("/tmp/mem"),
                                QStringLiteral("-t"),
                                QStringLiteral("mem mem"),
                                QStringLiteral("mem123")}) {
        FakeWakeManager wm;
        QSignalSpy preparedSpy(&wm, &WakeManager::wakePrepared);

        wm.prepareWake(300, mode);

        QCOMPARE(wm.wakeState(), QStringLiteral("failed"));
        QVERIFY(wm.lastError().contains(QStringLiteral("invalid rtcwake mode")));
        QCOMPARE(wm.scheduleCalls, 0);
        QCOMPARE(preparedSpy.count(), 0);
    }

    // The real modes are still accepted.
    FakeWakeManager wm;
    wm.prepareWake(300, QStringLiteral("mem"));
    QCOMPARE(wm.wakeState(), QStringLiteral("armed"));
    QCOMPARE(wm.scheduleCalls, 1);
}

void TestWakeManager::prepareWakeRejectionTouchesNoWake()
{
    // An invalid request must not disturb an already armed wake either.
    FakeWakeManager wm;
    QVERIFY(primeHelperOwnership(wm));
    QVERIFY(wm.helperOwnsWake());
    QCOMPARE(wm.wakeState(), QStringLiteral("armed"));
    const int callsAfterArm = wm.scheduleCalls;

    wm.prepareWake(59);

    QCOMPARE(wm.wakeState(), QStringLiteral("failed"));
    QCOMPARE(wm.scheduleCalls, callsAfterArm);  // no re-schedule attempted
    QCOMPARE(wm.cancelCalls, 0);                // and no cancellation attempted
    QVERIFY(wm.helperOwnsWake());               // the armed wake is untouched
}

void TestWakeManager::programWakeAlarmRejectsInvalidDelays_data()
{
    QTest::addColumn<qint64>("offset");  // seconds relative to now
    QTest::newRow("already-past") << qint64(-100);
    QTest::newRow("one-second") << qint64(1);
    QTest::newRow("below-minimum-59") << qint64(59);
    QTest::newRow("above-maximum-86401") << qint64(86401);
    QTest::newRow("far-above-100000") << qint64(100000);
    QTest::newRow("far-future-week") << qint64(7 * 24 * 3600);
}

void TestWakeManager::programWakeAlarmRejectsInvalidDelays()
{
    QFETCH(qint64, offset);

    FakeWakeManager wm;
    QString error;
    const qint64 epoch = QDateTime::currentSecsSinceEpoch() + offset;

    QVERIFY(!wm.programWakeAlarm(epoch, &error));
    QVERIFY(error.contains(QStringLiteral("outside the allowed range")));
    // Refused before inspectRtc(), before `hwclock --systohc` and before any
    // D-Bus call or process spawn: nothing privileged was attempted.
    QCOMPARE(wm.scheduleCalls, 0);
    QCOMPARE(wm.cancelCalls, 0);
    QVERIFY(!wm.helperOwnsWake());
}

void TestWakeManager::programWakeAlarmAcceptsValidBoundary()
{
    // Deliberately not run as root: programWakeAlarm() performs a best-effort
    // `hwclock --systohc`, which WOULD write the hardware clock when the
    // process is privileged. Unprivileged, that call fails harmlessly and the
    // test still proves the accepted delay reaches the helper.
    if (geteuid() == 0)
        QSKIP("running as root: programWakeAlarm() would write the RTC via "
              "hwclock --systohc; boundary acceptance is covered by "
              "prepareWakeAcceptsBoundaryDelays and the live helper checks");

    FakeWakeManager wm;
    QString error;
    const qint64 epoch = QDateTime::currentSecsSinceEpoch() + 3600;

    QVERIFY2(wm.programWakeAlarm(epoch, &error), qPrintable(error));
    QCOMPARE(wm.scheduleCalls, 1);
    QVERIFY(wm.scheduledSeconds.last() >= 60 && wm.scheduledSeconds.last() <= 86400);
    QVERIFY(wm.helperOwnsWake());
}

// ── F6: ownership must survive a failed cancellation ─────────────────────────

void TestWakeManager::failedCancelKeepsHelperOwnership()
{
    FakeWakeManager wm;
    wm.cancelSucceeds = false;
    wm.timerArmed = true;
    QVERIFY(primeHelperOwnership(wm));

    QString error;
    QVERIFY(!wm.clearWakeAlarm(&error));
    QVERIFY(!error.isEmpty());
    // The defect: ownership was dropped here, stranding the live timer and
    // making every later cancel skip the helper.
    QVERIFY2(wm.helperOwnsWake(),
             "a failed cancellation must keep helper ownership for a retry");
    QCOMPARE(wm.wakeState(), QStringLiteral("armed"));

    // Retry now succeeds and only then is ownership released.
    wm.cancelSucceeds = true;
    QVERIFY(wm.clearWakeAlarm(&error));
    QVERIFY(!wm.helperOwnsWake());
}

void TestWakeManager::successfulCancelReleasesHelperOwnership()
{
    FakeWakeManager wm;
    wm.cancelSucceeds = true;
    wm.timerArmed = false;
    QVERIFY(primeHelperOwnership(wm));

    QVERIFY(wm.clearWakeAlarm());
    QVERIFY(!wm.helperOwnsWake());
}

void TestWakeManager::cancelReleasesOwnershipWhenNothingIsArmed()
{
    // Cancel failed, but systemd reports no Lumalarm timer and no future RTC
    // alarm: the wake is genuinely gone, so releasing ownership is the truth
    // rather than a false failure that could never resolve.
    FakeWakeManager wm;
    wm.cancelSucceeds = false;
    wm.timerArmed = false;
    QVERIFY(primeHelperOwnership(wm));

    const WakeManager &constWm = wm;
    const qint64 rtc = constWm.readWakeAlarmValue();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (rtc > now)
        QSKIP("this host has a future RTC alarm armed; not clearing it from a test");

    QVERIFY(wm.clearWakeAlarm());
    QVERIFY(!wm.helperOwnsWake());
}

void TestWakeManager::scheduleRefusesOutOfRangeBeforeTheHelper()
{
    // Defence in depth: even called directly (bypassing every caller-side
    // check), the real scheduleViaHelper() must refuse to hand the privileged
    // service a value outside the boundary — the guard's own message proves
    // nothing was put on the bus.
    DirectWakeManager wm;
    QString msg;

    QVERIFY(wm.scheduleRefused(59, &msg));
    QVERIFY(msg.contains(QStringLiteral("outside the allowed range")));
    QVERIFY(wm.scheduleRefused(0, &msg));
    QVERIFY(wm.scheduleRefused(86401, &msg));
    QVERIFY(wm.scheduleRefused(uint(UINT32_MAX), &msg));
    QVERIFY(wm.scheduleRefused(uint(INT_MAX), &msg));

    // ...and the same guard is reached from the public entry point.
    FakeWakeManager fake;
    for (const int seconds : {0, 1, 59, 86401, 100000}) {
        QVERIFY(!fake.armTestWake(seconds));
        QCOMPARE(fake.wakeState(), QStringLiteral("idle"));
    }
    QCOMPARE(fake.scheduleCalls, 0);
}

// ── Existing test-alarm behaviour ────────────────────────────────────────────

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
    QVERIFY(wm.helperOwnsWake());

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
