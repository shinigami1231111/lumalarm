#include <QtTest>
#include <QDBusConnection>

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

QTEST_GUILESS_MAIN(TestNotifier)
#include "tst_notifier.moc"
