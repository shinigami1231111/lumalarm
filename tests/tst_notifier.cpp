#include <QDBusConnection>
#include <QDBusError>
#include <QDBusServer>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest>

#include "ui/Notifier.h"

// ============================================================================
// Unit tests for Notifier (src/ui/).
//
// The contract under test is "best effort, never crashes, never blocks": every
// entry point must be safe whether or not a session bus or notification daemon
// exists. The disabled path must additionally do no bus work at all.
// ============================================================================

class TestNotifier : public QObject
{
    Q_OBJECT

private slots:
    void constructionSucceedsWithoutBus();
    void notifyReturnsEarlyWhenDisabled();
    void notifyDoesNotCrashWithoutSessionBus();
    void notifyWakeFiredNeverWaitsForDaemon();
};

// The constructor performs no D-Bus work, so it is safe anywhere.
void TestNotifier::constructionSucceedsWithoutBus()
{
    Notifier enabled;
    QVERIFY(enabled.isEnabled());

    Notifier disabled(false);
    QVERIFY(!disabled.isEnabled());

    disabled.setEnabled(true);
    QVERIFY(disabled.isEnabled());
    disabled.setEnabled(false);
    QVERIFY(!disabled.isEnabled());
}

// Disabled: deliver() returns before touching the bus, so this is safe on any
// machine and must not log, notify or crash.
void TestNotifier::notifyReturnsEarlyWhenDisabled()
{
    Notifier n(false);

    n.notify(QStringLiteral("title"), QStringLiteral("body"));
    n.notifyAlarmUpcoming(QStringLiteral("Work"), 10);
    n.notifyWakeFired();
    n.notifyTestWakeArmed(60);

    QVERIFY(!n.isEnabled());
}

// With no session bus (headless / root shell) every call must fail softly.
void TestNotifier::notifyDoesNotCrashWithoutSessionBus()
{
    if (QDBusConnection::sessionBus().isConnected()) {
        QSKIP("session bus is available here, so the no-bus path cannot be exercised; "
              "the daemon path is covered by the manual busctl check");
    }

    Notifier n;
    n.notify(QStringLiteral("title"), QStringLiteral("body"));
    n.notifyAlarmUpcoming(QStringLiteral("Work"), 10);
    n.notifyWakeFired();
    n.notifyTestWakeArmed(60);

    QVERIFY(n.isEnabled());
}

// Regression: the alarm-trigger chain must not wait for a notification daemon.
// A peer bus with no notification object is connected but deliberately never
// answers Notify(), so the old blocking QDBusInterface::call() would stall here
// until its 25 s default timeout instead of letting QML show the ringing UI.
void TestNotifier::notifyWakeFiredNeverWaitsForDaemon()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), qPrintable(dir.errorString()));

    QDBusServer server(QStringLiteral("unix:path=%1/bus").arg(dir.path()));
    QVERIFY2(server.isConnected(), qPrintable(server.lastError().message()));

    const QDBusConnection peer = QDBusConnection::connectToPeer(
        server.address(), QStringLiteral("lumalarm-notifier-test"));
    QVERIFY2(peer.isConnected(), "the private peer bus must be connected");

    Notifier n(true, peer);
    QElapsedTimer timer;
    timer.start();
    n.notifyWakeFired();
    const qint64 elapsed = timer.elapsed();

    QVERIFY2(elapsed < 200,
             qPrintable(QStringLiteral("notifyWakeFired() blocked for %1 ms").arg(elapsed)));
}

QTEST_GUILESS_MAIN(TestNotifier)
#include "tst_notifier.moc"
