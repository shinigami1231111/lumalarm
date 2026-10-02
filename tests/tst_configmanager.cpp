#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

#include "managers/ConfigManager.h"

// ============================================================================
// Unit tests for ConfigManager (src/managers/). Covers construction defaults,
// settings round-trips through the QSettings ini file (including a fresh
// instance re-reading what a previous instance wrote), and the
// normalization of timePickerMode which is what keeps the alarm editor from
// hiding both picker controls.
//
// QStandardPaths test mode is enabled in initTestCase() so nothing here
// touches the real ~/.config/lumalarm.
// ============================================================================

class TestConfigManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void constructionCreatesConfigPathsAndDefaults();
    void defaultWakeModeRoundTripsThroughSettings();
    void timePickerModeIsNormalizedOnRead();
    void availableTonesReflectsTonesDirectory();

private:
    void cleanupTones();
    static QString settingsFilePath();
};

QString TestConfigManager::settingsFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/lumalarm/settings.ini");
}

void TestConfigManager::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    // Test-mode config dirs persist between runs, so drop settings.ini to
    // guarantee every run starts from ConfigManager's seeded defaults.
    QFile::remove(settingsFilePath());
}

void TestConfigManager::constructionCreatesConfigPathsAndDefaults()
{
    ConfigManager cm;

    // Config file path points into the (test-mode) config dir and the tones
    // directory is created by the constructor.
    QVERIFY(!cm.configFilePath().isEmpty());
    QVERIFY(cm.tonesDirectory().endsWith(QStringLiteral("tones")));
    QVERIFY2(QDir(cm.tonesDirectory()).exists(),
             qPrintable(QStringLiteral("tones dir missing: %1").arg(cm.tonesDirectory())));

    // Fresh defaults.
    QCOMPARE(cm.defaultWakeMode(), QStringLiteral("mem"));
    QCOMPARE(cm.timePickerMode(), QStringLiteral("wheel"));

    // ThemeManager is owned and wired up (theme changes surface as configChanged).
    QVERIFY(cm.theme() != nullptr);
}

void TestConfigManager::defaultWakeModeRoundTripsThroughSettings()
{
    ConfigManager cm;
    QSignalSpy spy(&cm, &ConfigManager::configChanged);

    cm.setDefaultWakeMode(QStringLiteral("rtc"));
    QCOMPARE(cm.defaultWakeMode(), QStringLiteral("rtc"));
    QCOMPARE(spy.count(), 1);

    // A brand-new ConfigManager over the same test config dir must read back
    // what the first one wrote — proves the value reaches the ini file.
    cm.setDefaultWakeMode(QStringLiteral("none"));
    ConfigManager reloaded;
    QCOMPARE(reloaded.defaultWakeMode(), QStringLiteral("none"));
}

void TestConfigManager::timePickerModeIsNormalizedOnRead()
{
    ConfigManager cm;

    cm.setTimePickerMode(QStringLiteral("spin"));
    QCOMPARE(cm.timePickerMode(), QStringLiteral("spin"));

    // Any other value (including the raw one before save) reads back as "wheel"
    // so the editor never ends up with neither picker visible.
    cm.setTimePickerMode(QStringLiteral("garbage"));
    QCOMPARE(cm.timePickerMode(), QStringLiteral("wheel"));
    cm.setTimePickerMode(QString());
    QCOMPARE(cm.timePickerMode(), QStringLiteral("wheel"));
}

void TestConfigManager::availableTonesReflectsTonesDirectory()
{
    ConfigManager cm;
    cleanupTones();
    QVERIFY(!cm.availableTones().contains(QStringLiteral("dummy-tone.wav")));

    QFile tone(cm.tonesDirectory() + QStringLiteral("/dummy-tone.wav"));
    QVERIFY(tone.open(QIODevice::WriteOnly));
    tone.write("RIFF");
    tone.close();

    // availableTones() returns file names of audio files in the tones dir.
    QVERIFY(cm.availableTones().contains(QStringLiteral("dummy-tone.wav")));

    cleanupTones();
}

void TestConfigManager::cleanupTones()
{
    const QString tonesDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/lumalarm/tones");
    QDir dir(tonesDir);
    if (dir.exists())
        dir.removeRecursively();
    QDir().mkpath(tonesDir);
}

QTEST_GUILESS_MAIN(TestConfigManager)
#include "tst_configmanager.moc"
