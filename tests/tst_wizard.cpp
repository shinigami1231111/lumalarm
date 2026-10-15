#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "audio/AlarmAudioController.h"
#include "managers/AlarmManager.h"
#include "managers/ConfigManager.h"
#include "ui/WizardController.h"
#include "wake/WakeManager.h"

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
    bool armProbeWake() override
    {
        return probeResult;
    }
};

const char *kNotificationsService = "org.freedesktop.Notifications";

bool notificationDaemonPresent()
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface() &&
           bus.interface()->isServiceRegistered(QLatin1String(kNotificationsService));
}

// Profile paths the wizard is expected to leave alone.
QString profileDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           QStringLiteral("/lumalarm");
}

QString alarmsPath()
{
    return profileDir() + QStringLiteral("/alarms.json");
}

QString themePath()
{
    return profileDir() + QStringLiteral("/theme.conf");
}

// Callers that care whether the file existed pair this with QFile::exists();
// here "absent" and "empty" are both just "no bytes".
QByteArray readBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
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
    void nextWalksAllSteps();
    void nextOnLastStepIsANoop();
    void backOnFirstStepIsANoop();
    void skipPersistsAndEmitsExactlyOnce();
    void finishPersistsAndEmitsExactlyOnce();
    void finishTwiceEmitsOnlyOnce();
    void completedFlagRoundTripsThroughDisk();
    void genuineFreshProfileOpensWizard();
    void establishedProfileIsNotPromptedAgain();
    void completedWizardDoesNotReopen();
    void forceShowOverridesPersistedFlag();
    void languageSelectionUpdatesConfiguration();
    void testWakeSuccessUnlocksNext();
    void testWakeFailureLeavesNextLocked();
    void testNotificationReportsBusTruthfully();
    void availableSoundsAreEnumerated();
    void defaultSoundRoundTrips();
    void skipLeavesConfigurationIntact();
    void wizardRunNeverTouchesAlarms();

private:
    ConfigManager *m_config = nullptr;
    WakeManager *m_wake = nullptr;
    AlarmAudioController *m_audio = nullptr;
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
    // Production controller WITHOUT start(): no backend probe, no recovery,
    // no CLI, no real audio unless preview() resolves a tone — and the tests
    // never call previewTone().
    m_audio = new AlarmAudioController(this);
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
    QCOMPARE(w.stepCount(), 6);
    QCOMPARE(w.wizardCompleted(), false);
    QCOMPARE(w.currentStep(), int(0));
}

void TestWizard::nextWalksAllSteps()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    QSignalSpy spy(&w, &WizardController::currentStepChanged);

    // Only the wake page (3) gates Next; it is unlocked by a skip so the walk
    // stays deterministic regardless of the host's wake capabilities.
    w.skipWakeStep();
    for (int i = 0; i < WizardController::kStepCount; ++i)
        w.next();

    QCOMPARE(w.currentStep(), WizardController::kStepCount - 1);
    QVERIFY(spy.count() >= WizardController::kStepCount - 1);
}

void TestWizard::nextOnLastStepIsANoop()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    // Reach the last step first: the wake page deliberately gates Next, and
    // this case is about what happens *at* the end, not about that gate.
    w.skipWakeStep();
    for (int i = 0; i < WizardController::kStepCount; ++i)
        w.next();
    QCOMPARE(w.currentStep(), WizardController::kStepCount - 1);

    QSignalSpy spy(&w, &WizardController::currentStepChanged);
    w.next();
    w.next();
    QCOMPARE(w.currentStep(), WizardController::kStepCount - 1);
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
    QCOMPARE(spy.count(), 1); // guard: no duplicate emission
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

void TestWizard::genuineFreshProfileOpensWizard()
{
    // A profile with neither settings.ini nor theme.conf has never been used by
    // any Lumalarm release, so it is a first run and must open the wizard.
    //
    // m_config goes first: its ThemeManager holds a file watcher that would put
    // theme.conf straight back before the constructor below could look for it.
    delete m_config;
    m_config = nullptr;

    const QString dir = profileDir();
    QFile::remove(dir + QStringLiteral("/settings.ini"));
    QFile::remove(themePath());

    bool reportedFirstRun = false;
    bool overlayVisible = false;
    int landingStep = -1;
    {
        ConfigManager fresh;
        ProbeableWizard w(&fresh, m_wake, m_audio);
        reportedFirstRun = !fresh.wizardCompleted();
        overlayVisible = !w.wizardCompleted();
        landingStep = w.currentStep();
    }

    // Put an instance back before asserting anything — cleanup() needs one.
    m_config = new ConfigManager(this);

    QVERIFY2(reportedFirstRun, "a brand-new profile must report wizardCompleted()==false");
    QVERIFY2(overlayVisible, "a brand-new profile must show the wizard overlay");
    QCOMPARE(landingStep, 0);
}

void TestWizard::establishedProfileIsNotPromptedAgain()
{
    // A profile written by a release that predates the wizard: settings.ini is
    // there (init() just wrote one) but the wizard key does not exist at all.
    {
        QSettings raw(m_config->configFilePath(), QSettings::IniFormat);
        raw.remove(QStringLiteral("wizard/completed"));
        raw.sync();
    }
    QVERIFY(QFile::exists(m_config->configFilePath()));

    ConfigManager established;
    ProbeableWizard w(&established, m_wake, m_audio);

    QVERIFY2(established.wizardCompleted(),
             "an upgrade must not report an existing profile as a first run");
    QVERIFY2(w.wizardCompleted(), "the overlay must stay hidden for an existing profile");
}

void TestWizard::completedWizardDoesNotReopen()
{
    {
        ProbeableWizard w(m_config, m_wake, m_audio);
        w.finish();
        QVERIFY(w.wizardCompleted());
    }

    // The next launch is a fresh controller over the same file: nothing forces
    // the overlay back on.
    ProbeableWizard again(m_config, m_wake, m_audio);
    QVERIFY(again.wizardCompleted());

    // ...and Settings > "Run setup wizard again" is still the way to reopen it.
    again.reset();
    QVERIFY(!again.wizardCompleted());
    QVERIFY(!m_config->wizardCompleted());
    QCOMPARE(again.currentStep(), 0);
}

void TestWizard::forceShowOverridesPersistedFlag()
{
    m_config->setWizardCompleted(true);

    ProbeableWizard w(m_config, m_wake, m_audio);
    QVERIFY(w.wizardCompleted()); // nothing forced yet

    w.setForceShow(true);
    QCOMPARE(w.wizardCompleted(), false);        // --show-wizard wins ...
    QCOMPARE(m_config->wizardCompleted(), true); // ... but the disk key is untouched

    // An explicit finish ends the override and persists normally.
    QSignalSpy spy(&w, &WizardController::wizardCompletedChanged);
    w.finish();
    QCOMPARE(w.wizardCompleted(), true);
    QCOMPARE(spy.count(), 1);
}

void TestWizard::languageSelectionUpdatesConfiguration()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    w.setLanguage(QString()); // deterministic starting point

    QSignalSpy spy(&w, &WizardController::languageChanged);
    QCOMPARE(spy.count(), 0);

    w.setLanguage(QStringLiteral("ar"));
    QCOMPARE(w.language(), QStringLiteral("ar"));
    QCOMPARE(m_config->language(), QStringLiteral("ar")); // one shared key
    QCOMPARE(spy.count(), 1);

    w.setLanguage(QStringLiteral("ar")); // unchanged -> silent
    QCOMPARE(spy.count(), 1);

    w.setLanguage(QStringLiteral("en"));
    QCOMPARE(w.language(), QStringLiteral("en"));
    QCOMPARE(m_config->language(), QStringLiteral("en"));
    QCOMPARE(spy.count(), 2);

    w.setLanguage(QString()); // back to "follow the system locale"
    QCOMPARE(spy.count(), 3);
    QVERIFY(m_config->language().isEmpty());
}

void TestWizard::testWakeSuccessUnlocksNext()
{
    ProbeableWizard page(m_config, m_wake, m_audio);
    // welcome -> language -> sound -> wake method
    for (int i = 0; i < WizardController::WakeMethod; ++i)
        page.next();
    QCOMPARE(page.currentStep(), int(WizardController::WakeMethod));
    QCOMPARE(page.canGoNext(), false); // locked before any probe

    QSignalSpy stepSpy(&page, &WizardController::currentStepChanged);
    QSignalSpy statusSpy(&page, &WizardController::wakeStatusChanged);

    page.probeResult = true;
    const bool outcome = page.testWake();

    QCOMPARE(outcome, true);
    QCOMPARE(page.canGoNext(), true); // gate opened
    QVERIFY(stepSpy.count() >= 1);
    QVERIFY(statusSpy.count() >= 1);
    QVERIFY(page.wakeStatus().contains(QStringLiteral("succeeded")));
}

void TestWizard::testWakeFailureLeavesNextLocked()
{
    ProbeableWizard page(m_config, m_wake, m_audio);
    // welcome -> language -> sound -> wake method
    for (int i = 0; i < WizardController::WakeMethod; ++i)
        page.next();
    QCOMPARE(page.currentStep(), int(WizardController::WakeMethod));
    QCOMPARE(page.canGoNext(), false);

    QSignalSpy stepSpy(&page, &WizardController::currentStepChanged);

    page.probeResult = false;
    const bool outcome = page.testWake();

    QCOMPARE(outcome, false);
    QCOMPARE(page.canGoNext(), false); // still locked
    QCOMPARE(stepSpy.count(), 0);      // gate did not open
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
        if (entry.toMap().value(QStringLiteral("id")).toString() ==
            QLatin1String("wizard-test-tone.wav"))
            found = true;
    QVERIFY2(found, "seeded tone missing from availableSounds()");
}

void TestWizard::defaultSoundRoundTrips()
{
    ProbeableWizard w(m_config, m_wake, m_audio);
    const QString before = w.defaultSound();
    QVERIFY(!before.isEmpty()); // default = first available tone

    QSignalSpy spy(&w, &WizardController::defaultSoundChanged);
    w.setDefaultSound(QStringLiteral("other-tone.mp3"));
    QCOMPARE(w.defaultSound(), QStringLiteral("other-tone.mp3"));
    QCOMPARE(spy.count(), 1);

    // Same value again: no emission.
    w.setDefaultSound(QStringLiteral("other-tone.mp3"));
    QCOMPARE(spy.count(), 1);
}

void TestWizard::skipLeavesConfigurationIntact()
{
    // Unrelated configuration the wizard has no business touching.
    const QString wakeBefore = m_config->sleepMode();
    const double opacityBefore = m_config->themeOpacity();
    const QString languageBefore = m_config->language();
    m_config->setSleepMode(QStringLiteral("disk"));
    m_config->setThemeOpacity(0.42);

    const bool alarmsExisted = QFile::exists(alarmsPath());
    const QByteArray alarmsBefore = readBytes(alarmsPath());
    const QByteArray themeBefore = readBytes(themePath());

    // Walk partway (so cancel/skip after real navigation is what is under
    // test), change the language, then abandon the setup.
    ProbeableWizard w(m_config, m_wake, m_audio);
    w.next();
    w.setLanguage(QStringLiteral("ar"));
    w.next();
    w.back();
    w.skip();

    QVERIFY(w.wizardCompleted());
    QVERIFY(m_config->wizardCompleted());

    // Every unrelated setting reads back exactly as it did.
    QCOMPARE(m_config->sleepMode(), QStringLiteral("disk"));
    QCOMPARE(m_config->themeOpacity(), 0.42);
    QCOMPARE(m_config->language(), QStringLiteral("ar"));

    // Neither the alarm store nor the theme file was opened for writing.
    QCOMPARE(QFile::exists(alarmsPath()), alarmsExisted);
    QCOMPARE(readBytes(alarmsPath()), alarmsBefore);
    QCOMPARE(readBytes(themePath()), themeBefore);

    // Put the settings back so nothing leaks into the other test binaries.
    m_config->setSleepMode(wakeBefore);
    m_config->setThemeOpacity(opacityBefore);
    m_config->setLanguage(languageBefore);
}

void TestWizard::wizardRunNeverTouchesAlarms()
{
    const bool existedBefore = QFile::exists(alarmsPath());
    const QByteArray original = readBytes(alarmsPath());

    int countBefore = -1;
    int countAfter = -1;
    bool storeUnchanged = false;
    {
        // Real store: loading may create alarms.json when it was absent, so the
        // comparison starts after the alarm below has been seeded.
        AlarmManager alarms;
        QVariantMap seed;
        seed.insert(QStringLiteral("hour"), 7);
        seed.insert(QStringLiteral("minute"), 30);
        seed.insert(QStringLiteral("name"), QStringLiteral("keep me"));
        alarms.addAlarm(seed);
        countBefore = alarms.alarmList().size();

        const QByteArray seeded = readBytes(alarmsPath());

        ProbeableWizard w(m_config, m_wake, m_audio);
        w.probeResult = true;
        w.testWake(); // STG-5.1 test mode, no alarm by design
        w.finish();

        countAfter = alarms.alarmList().size();
        storeUnchanged = (readBytes(alarmsPath()) == seeded);
    }

    // Other test binaries share this profile — restore it byte for byte.
    if (existedBefore) {
        QFile f(alarmsPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(original);
    } else {
        QFile::remove(alarmsPath());
    }

    QVERIFY2(countBefore >= 1, "the seeded alarm never made it into the store");
    QCOMPARE(countAfter, countBefore);
    QVERIFY2(storeUnchanged, "a wizard run must not add to or rewrite the alarm store");
}

QTEST_GUILESS_MAIN(TestWizard)
#include "tst_wizard.moc"
