#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QSettings>
#include <QFile>
#include <QDir>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "managers/ConfigManager.h"
#include "wake/WakeManager.h"
#include "audio/AudioPlayer.h"
#include "ui/WizardController.h"

// ============================================================================
// Unit tests for WizardController (src/ui/) — the STG-5.5 first-run wizard.
//
// The privileged half of testWake() (arming a real wake through the D-Bus
// helper / polkit) cannot run in a test, so the class exposes a protected
// virtual armProbeWake() seam. ProbeableWizard overrides it to supply both
// outcomes; everything the controller does WITH that result — the gate, the
// status line, the immediate cancel — is the real code.
//
// The notification probe is checked against an independent session-bus probe
// instead of a hardcoded expectation, so the test is honest whether or not the
// machine running it has a notification daemon.
// ============================================================================

namespace {
// Substitutes only the D-Bus/polkit-dependent arming call.
class ProbeableWizard : public WizardController
{
public:
    using WizardController::WizardController;
    bool probeResult = false;

protected:
    bool armProbeWake() override { return probeResult; }
};

const char *kNotificationsService = "org.freedesktop.Notifications";

bool notificationDaemonPresent()
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface()
        && bus.interface()->isServiceRegistered(QLatin1String(kNotificationsService));
}
} // namespace

class TestWizard : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void initialStepIsZeroAndCannotGoBack();
    void nextWalksAllFiveSteps();
    void nextOnLastStepIsANoop();
    void backOnFirstStepIsANoop();
    void skipPersistsAndEmitsExactlyOnce();
    void finishPersistsAndEmitsExactlyOnce();
    void finishTwiceEmitsOnlyOnce();
    void completedFlagRoundTripsThroughDisk();
    void freshConfigIsNotCompleted();
    void forceShowOverridesPersistedFlag();
    void testWakeSuccessUnlocksNext();
    void testWakeFailureLeavesNextLocked();
    void testNotificationReportsBusTruthfully();
    void availableSoundsAreEnumerated();
    void defaultSoundRoundTrips();

private:
    ConfigManager *m_config = nullptr;
    WakeManager *m_wake = nullptr;
    AudioPlayer *m_audio = nullptr;
};

void TestWizard::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestWizard::init()
{
    // Drop whatever the previous case used so two ConfigManager instances are
    // never alive on the same settings file at once.
    delete m_config;
    delete m_wake;
    delete m_audio;
    m_config = nullptr;
    m_wake = nullptr;
    m_audio = nullptr;

    {
        ConfigManager cfg;
        cfg.setWizardCompleted(false);
        // Seed the tone catalogue so availableSounds() has something to report
        // and the "default = first item" contract is testable.
        QFile f(cfg.tonesDirectory() + QStringLiteral("/wizard-test-tone.wav"));
        if (f.open(QIODevice::WriteOnly))
            f.write("RIFF");
    }
    m_config = new ConfigManager(this);
    m_wake = new WakeManager(this);
    m_audio = new AudioPlayer(this);
}

void TestWizard::cleanup()
{
    QFile::remove(m_config->tonesDirectory() + QStringLiteral("/wizard-test-tone.wav"));
    delete m_config;
    delete m_wake;
    delete m_audio;
    m_config = nullptr;
    m_wake = nullptr;
    m_audio = nullptr;
}

void TestWizard::initialStepIsZeroAndCannotGoBack()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QCOMPARE(w.currentStep(), 0);
    QCOMPARE(w.canGoBack(), false);
    QCOMPARE(w.stepCount(), 5);
    QCOMPARE(w.wizardCompleted(), false);
    QCOMPARE(w.currentStep(), int(0));
}

void TestWizard::nextWalksAllFiveSteps()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QSignalSpy spy(&w, &WizardController::currentStepChanged);

    // Steps 1..4 need no gate; the wake page (1) is unlocked by a skip so the
    // walk stays deterministic regardless of the host's wake capabilities.
    w.skipWakeStep();
    for (int i = 0; i < 5; ++i)
        w.next();

    QCOMPARE(w.currentStep(), 4);
    QVERIFY(spy.count() >= 4);
}

void TestWizard::nextOnLastStepIsANoop()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    // Reach the last step first: the wake page deliberately gates Next, and
    // this case is about what happens *at* the end, not about that gate.
    w.skipWakeStep();
    for (int i = 0; i < 5; ++i)
        w.next();
    QCOMPARE(w.currentStep(), 4);

    QSignalSpy spy(&w, &WizardController::currentStepChanged);
    w.next();
    w.next();
    QCOMPARE(w.currentStep(), 4);
    QCOMPARE(spy.count(), 0);
}

void TestWizard::backOnFirstStepIsANoop()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QSignalSpy spy(&w, &WizardController::currentStepChanged);
    w.back();
    w.back();
    QCOMPARE(w.currentStep(), 0);
    QCOMPARE(spy.count(), 0);
}

void TestWizard::skipPersistsAndEmitsExactlyOnce()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QSignalSpy spy(&w, &WizardController::wizardCompletedChanged);

    w.skip();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(w.wizardCompleted(), true);
    QCOMPARE(m_config->wizardCompleted(), true);

    w.skip();
    QCOMPARE(spy.count(), 1);           // guard: no duplicate emission
}

void TestWizard::finishPersistsAndEmitsExactlyOnce()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QSignalSpy spy(&w, &WizardController::wizardCompletedChanged);

    w.finish();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(w.wizardCompleted(), true);
    QCOMPARE(m_config->wizardCompleted(), true);
}

void TestWizard::finishTwiceEmitsOnlyOnce()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QSignalSpy spy(&w, &WizardController::wizardCompletedChanged);

    w.finish();
    w.finish();
    w.finish();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_config->wizardCompleted(), true);
}

void TestWizard::completedFlagRoundTripsThroughDisk()
{
    {
        ProbeableWizard w(m_config, m_wake, m_audio);
        w.finish();
    }
    // A brand-new reader over the same file must still see the flag.
    ConfigManager reloaded;
    QVERIFY(reloaded.wizardCompleted());
}

void TestWizard::freshConfigIsNotCompleted()
{
    // Remove the key entirely rather than writing false: a profile that has
    // never seen the wizard must show it.
    {
        QSettings raw(m_config->configFilePath(), QSettings::IniFormat);
        raw.remove(QStringLiteral("wizard/completed"));
        raw.sync();
    }
    ConfigManager fresh;
    QVERIFY(!fresh.wizardCompleted());
}

void TestWizard::forceShowOverridesPersistedFlag()
{
    m_config->setWizardCompleted(true);

    ProbeableWizard w(m_config, m_wake, m_audio);
    QVERIFY(w.wizardCompleted());       // nothing forced yet

    w.setForceShow(true);
    QCOMPARE(w.wizardCompleted(), false);   // --show-wizard wins ...
    QCOMPARE(m_config->wizardCompleted(), true);   // ... but the disk key is untouched

    // An explicit finish ends the override and persists normally.
    QSignalSpy spy(&w, &WizardController::wizardCompletedChanged);
    w.finish();
    QCOMPARE(w.wizardCompleted(), true);
    QCOMPARE(spy.count(), 1);
}

void TestWizard::testWakeSuccessUnlocksNext()
{
    ProbeableWizard page(m_config, m_wake, m_audio);
    page.next();                       // welcome -> wake method
    QCOMPARE(page.currentStep(), 1);
    QCOMPARE(page.canGoNext(), false); // locked before any probe

    QSignalSpy stepSpy(&page, &WizardController::currentStepChanged);
    QSignalSpy statusSpy(&page, &WizardController::wakeStatusChanged);

    page.probeResult = true;
    const bool outcome = page.testWake();

    QCOMPARE(outcome, true);
    QCOMPARE(page.canGoNext(), true);          // gate opened
    QVERIFY(stepSpy.count() >= 1);
    QVERIFY(statusSpy.count() >= 1);
    QVERIFY(page.wakeStatus().contains(QStringLiteral("succeeded")));
}

void TestWizard::testWakeFailureLeavesNextLocked()
{
    ProbeableWizard page(m_config, m_wake, m_audio);
    page.next();                       // welcome -> wake method
    QCOMPARE(page.currentStep(), 1);
    QCOMPARE(page.canGoNext(), false);

    QSignalSpy stepSpy(&page, &WizardController::currentStepChanged);

    page.probeResult = false;
    const bool outcome = page.testWake();

    QCOMPARE(outcome, false);
    QCOMPARE(page.canGoNext(), false);        // still locked
    QCOMPARE(stepSpy.count(), 0);             // gate did not open
    QVERIFY(page.wakeStatus().contains(QStringLiteral("failed")));
}

void TestWizard::testNotificationReportsBusTruthfully()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    const bool daemonThere = notificationDaemonPresent();
    const bool sent = w.testNotification();

    if (!daemonThere) {
        // The required case: no daemon -> false, no crash, no hang.
        QVERIFY2(!sent, "testNotification() claimed success with no daemon on the bus");
        QCOMPARE(w.notificationsAvailable(), false);
    } else {
        QVERIFY2(sent, "testNotification() failed although a daemon is present");
        QCOMPARE(w.notificationsAvailable(), true);
    }
}

void TestWizard::availableSoundsAreEnumerated()
{
    // availableSounds() reads the app's own tone catalogue; init() seeded it
    // with wizard-test-tone.wav.
    ProbeableWizard w(m_config, m_wake, m_audio);
    const QVariantList sounds = w.availableSounds();
    QVERIFY(!sounds.isEmpty());

    bool found = false;
    for (const QVariant &entry : sounds)
        if (entry.toMap().value(QStringLiteral("id")).toString() == QLatin1String("wizard-test-tone.wav"))
            found = true;
    QVERIFY2(found, "seeded tone missing from availableSounds()");
}

void TestWizard::defaultSoundRoundTrips()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    const QString before = w.defaultSound();
    QVERIFY(!before.isEmpty());          // default = first available tone

    QSignalSpy spy(&w, &WizardController::defaultSoundChanged);
    w.setDefaultSound(QStringLiteral("other-tone.mp3"));
    QCOMPARE(w.defaultSound(), QStringLiteral("other-tone.mp3"));
    QCOMPARE(spy.count(), 1);

    // Same value again: no emission.
    w.setDefaultSound(QStringLiteral("other-tone.mp3"));
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TestWizard)
#include "tst_wizard.moc"
