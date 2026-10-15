#include <QApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QtTest>

#include "managers/AlarmManager.h"
#include "ui/TrayController.h"
#include "wake/Scheduler.h"
#include "wake/WakeManager.h"

// ============================================================================
// Unit tests for TrayIcon (src/ui/).
//
// Runs on the offscreen platform plugin so it works headless: there is no
// system tray there, which is exactly the case the class must survive. The
// tooltip text is still computed and assertable because it is pure logic.
//
// A custom main() is required: QSystemTrayIcon needs a QApplication, and the
// platform plugin has to be chosen before the application object exists.
// ============================================================================

class TestTrayIcon : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void constructionIsSafeWithoutSystemTray();
    void tooltipWithoutAlarmsReportsNone();
    void tooltipNamesTheNextAlarm();

private:
    AlarmManager *m_alarms = nullptr;
    WakeManager *m_wake = nullptr;
    Scheduler *m_scheduler = nullptr;
};

void TestTrayIcon::initTestCase()
{
    // Keep AlarmManager's persistence inside the test config dir.
    QStandardPaths::setTestModeEnabled(true);
    m_alarms = new AlarmManager(this);
    m_wake = new WakeManager(this);
    m_scheduler = new Scheduler(m_alarms, m_wake, QString(), this);
}

void TestTrayIcon::constructionIsSafeWithoutSystemTray()
{
    TrayController tray(m_alarms->model(), m_scheduler);

    // Nothing is created until it is enabled, so it must not claim usability.
    QVERIFY(!tray.usable());

    // Enabling with no tray host must degrade quietly, and usable() must agree
    // with what the platform actually reports (it is DYNAMIC, not cached).
    tray.setEnabled(true);
    QCOMPARE(tray.usable(), QSystemTrayIcon::isSystemTrayAvailable());

    tray.setEnabled(false);
    QVERIFY(!tray.usable());

    // shutdown() is idempotent and leaves the object inert.
    tray.shutdown();
    tray.shutdown();
    QVERIFY(!tray.usable());
}

void TestTrayIcon::tooltipWithoutAlarmsReportsNone()
{
    // Null scheduler is the degenerate case; it must never crash.
    TrayController detached(nullptr, nullptr);
    QCOMPARE(detached.tooltip(), QStringLiteral("No alarm scheduled"));

    m_alarms->setAlarms({});
    TrayController tray(m_alarms->model(), m_scheduler);
    QCOMPARE(tray.tooltip(), QStringLiteral("No alarm scheduled"));
}

void TestTrayIcon::tooltipNamesTheNextAlarm()
{
    // Target ~90 minutes out so the margin below can never roll the hour over.
    const QDateTime target = QDateTime::currentDateTime().addSecs(90 * 60);
    Alarm a;
    a.hour = target.time().hour();
    a.minute = target.time().minute();
    for (int d = 0; d < 7; ++d)
        a.days[d] = true; // every day: the occurrence exists today or tomorrow
    a.enabled = true;
    m_alarms->setAlarms({a});

    TrayController tray(m_alarms->model(), m_scheduler);
    const QString tip = tray.tooltip();

    // Computed from the alarm's own fields, not from the implementation.
    const QString expected = QStringLiteral("%1:%2").arg(a.hour).arg(a.minute, 2, 10, QChar('0'));

    QVERIFY2(tip.startsWith(QStringLiteral("Next alarm: ")), qPrintable(tip));
    QVERIFY2(tip.contains(expected), qPrintable(tip));
    // Not armed here, so no "(in N min)" suffix.
    QVERIFY(!tip.contains(QStringLiteral("in ")));
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestTrayIcon test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_trayicon.moc"
