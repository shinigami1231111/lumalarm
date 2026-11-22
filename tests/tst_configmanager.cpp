#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "managers/ConfigManager.h"
#include "managers/TonePaths.h"

// ============================================================================
// Unit tests for ConfigManager (src/managers/). Covers construction defaults,
// settings round-trips through the QSettings ini file (including a fresh
// instance re-reading what a previous instance wrote), and the
// normalization of timePickerMode which is what keeps the alarm editor from
// hiding both picker controls.
//
// Issue #2 (sound discovery / import) adds: tone enumeration across the user
// and the package-installed system folders, the advertised format list, and
// every import/delete outcome — success, each rejection reason, the duplicate
// "ask" policy and names with spaces, '#', '%' and Arabic characters.
//
// QStandardPaths test mode is enabled in initTestCase() so nothing here
// touches the real ~/.config/lumalarm, and XDG_DATA_DIRS is pointed at a
// private QTemporaryDir so the system-tone cases never read /usr/share either.
// ============================================================================

namespace {
bool writeFile(const QString &path, const QByteArray &content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(content);
    return true;
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}
} // namespace

class TestConfigManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void constructionCreatesConfigPathsAndDefaults();
    void defaultWakeModeWasRemovedInFavourOfSleepMode();
    void timePickerModeIsNormalizedOnRead();
    void timeFormatIsNormalizedOnRead();
    void availableTonesReflectsTonesDirectory();
    void availableTonesIncludesSystemToneFolder();
    void availableTonesCoversEveryAdvertisedFormat();
    void mixedCaseExtensionsImportAndAppearInDiscovery();
    void copyToTonesImportsFromUrlWithSpacesAndUnicode();
    void copyToTonesRejectsBadInput();
    void duplicateImportAsksAndReplaceOverwrites();
    void deleteToneRefusesSystemTones();
    void listedTonesResolveForPlayback();
    void testWakeDurationDefaultsAndClamps();
    void audioSettingsRoundTripAndClamp();
    void emojiLevelDefaultsToMoreAndPersists();
    void emojiLevelChangeHasDedicatedSignalAndMigratesLegacyOff();

private:
    void cleanupTones();
    static QString settingsFilePath();
    QString systemTonesDirPath() const;

    // Stand-in for XDG_DATA_DIRS: the only "system" tone folder these tests
    // can see, and a temporary one at that.
    QTemporaryDir m_dataDirs;
};

QString TestConfigManager::settingsFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           QStringLiteral("/lumalarm/settings.ini");
}

QString TestConfigManager::systemTonesDirPath() const
{
    return m_dataDirs.path() + QStringLiteral("/lumalarm/tones");
}

void TestConfigManager::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // The system tone folders come from XDG_DATA_DIRS. Point it at a private
    // temp dir before anything constructs a ConfigManager, so the system-tone
    // cases below run entirely inside it instead of reading /usr/share.
    QVERIFY(m_dataDirs.isValid());
    qputenv("XDG_DATA_DIRS", m_dataDirs.path().toLocal8Bit());
    QVERIFY2(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)
                 .contains(m_dataDirs.path()),
             "XDG_DATA_DIRS override not visible to Qt — the system tone tests "
             "would otherwise depend on the machine's real filesystem");

    // Test-mode config dirs persist between runs, so drop settings.ini to
    // guarantee every run starts from ConfigManager's seeded defaults.
    QFile::remove(settingsFilePath());
    cleanupTones();
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

    // Fresh defaults. sleepMode is the ONLY mode setting: it is what the
    // helper runs as `rtcwake -m <mode>`.
    QCOMPARE(cm.sleepMode(), QStringLiteral("mem"));
    QCOMPARE(cm.timePickerMode(), QStringLiteral("wheel"));

    // ThemeManager is owned and wired up (theme changes surface as configChanged).
    QVERIFY(cm.theme() != nullptr);
}

void TestConfigManager::defaultWakeModeWasRemovedInFavourOfSleepMode()
{
    ConfigManager cm;
    QSignalSpy spy(&cm, &ConfigManager::configChanged);

    // sleepMode is validated against the GUI method list, so only a mode the
    // selector offers can be stored — it is the value the helper runs as
    // `rtcwake -m <mode>`, and the only mode setting there is. The old
    // `defaultWakeMode` property was rendered in Settings but read by nothing,
    // and offered a hardcoded list containing "none", which is not a valid
    // rtcwake mode at all; it was removed on 2026-10-08.
    QCOMPARE(cm.sleepModeChoices(),
             QStringList({QStringLiteral("mem"), QStringLiteral("freeze"),
                          QStringLiteral("standby"), QStringLiteral("off"),
                          QStringLiteral("disk")}));
    cm.setSleepMode(QStringLiteral("standby"));
    QCOMPARE(cm.sleepMode(), QStringLiteral("standby"));
    QCOMPARE(spy.count(), 1);

    // A brand-new ConfigManager over the same test config dir must read back
    // what the first one wrote — proves the value reaches the ini file.
    ConfigManager reloaded;
    QCOMPARE(reloaded.sleepMode(), QStringLiteral("standby"));

    // Off-list modes are never stored: they fall back to the default, so a
    // stray or hand-edited ini value cannot reach the privileged helper.
    // `no` / `on` stay valid in the helper allow-list but were removed from
    // the selector on 2026-11-22 because they do not select a sleep state.
    for (const QString &removed : {QStringLiteral("none"), QStringLiteral("no"),
                                   QStringLiteral("on")}) {
        cm.setSleepMode(removed);
        QCOMPARE(cm.sleepMode(), QStringLiteral("mem"));
    }
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

void TestConfigManager::timeFormatIsNormalizedOnRead()
{
    ConfigManager cm;
    QCOMPARE(cm.timeFormat(), QStringLiteral("24h")); // general numbers by default

    cm.setTimeFormat(QStringLiteral("12h"));
    QCOMPARE(cm.timeFormat(), QStringLiteral("12h"));
    ConfigManager reloaded;
    QCOMPARE(reloaded.timeFormat(), QStringLiteral("12h"));

    cm.setTimeFormat(QStringLiteral("am/pm")); // anything else is 24-hour
    QCOMPARE(cm.timeFormat(), QStringLiteral("24h"));
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

void TestConfigManager::availableTonesIncludesSystemToneFolder()
{
    ConfigManager cm;
    cleanupTones();

    const QString systemDir = systemTonesDirPath();
    QVERIFY2(QDir().mkpath(systemDir), qPrintable(systemDir));

    QVERIFY(writeFile(systemDir + QStringLiteral("/shared-tone.wav"), "RIFF"));
    QVERIFY(writeFile(cm.tonesDirectory() + QStringLiteral("/mine.wav"), "RIFF"));

    // The package-installed folder is reported separately from the user one.
    QCOMPARE(cm.systemTonesDirectory(), systemDir);
    QVERIFY(cm.tonesDirectory() != systemDir);

    const QStringList tones = cm.availableTones();
    QVERIFY2(tones.contains(QStringLiteral("shared-tone.wav")),
             "a tone in the XDG data folder must be discovered");
    QVERIFY2(tones.contains(QStringLiteral("mine.wav")),
             "the user folder must keep working alongside it");

    // A user file of the same name shadows the packaged one instead of
    // appearing twice: one name, one entry, and playback resolves to ours.
    QVERIFY(writeFile(cm.tonesDirectory() + QStringLiteral("/shared-tone.wav"), "RIFF"));
    QCOMPARE(cm.availableTones().count(QStringLiteral("shared-tone.wav")), 1);
    QCOMPARE(TonePaths::resolve(QStringLiteral("shared-tone.wav")),
             cm.tonesDirectory() + QStringLiteral("/shared-tone.wav"));

    cleanupTones();
    QCOMPARE(cm.systemTonesDirectory(), QString());
    QVERIFY(!cm.availableTones().contains(QStringLiteral("shared-tone.wav")));
    QVERIFY(!cm.availableTones().contains(QStringLiteral("mine.wav")));
}

void TestConfigManager::availableTonesCoversEveryAdvertisedFormat()
{
    ConfigManager cm;
    cleanupTones();

    // Exactly the set the import dialog's nameFilters advertises and the
    // README documents. Adding a sixth to one side without the other is the
    // bug that let .aac import and then vanish from the list.
    const QStringList expected = {
        QStringLiteral("a.wav"),  QStringLiteral("b.mp3"), QStringLiteral("c.ogg"),
        QStringLiteral("d.flac"), QStringLiteral("e.aac"),
    };
    for (const QString &name : expected)
        QVERIFY(writeFile(cm.tonesDirectory() + QLatin1Char('/') + name, "RIFF"));

    // A non-audio file dropped into the folder is ignored, not offered.
    QVERIFY(writeFile(cm.tonesDirectory() + QStringLiteral("/notes.txt"), "text"));

    const QStringList tones = cm.availableTones();
    for (const QString &name : expected)
        QVERIFY2(tones.contains(name), qPrintable(QStringLiteral("missing: %1").arg(name)));
    QVERIFY2(!tones.contains(QStringLiteral("notes.txt")), "a .txt must not be discovered");

    QCOMPARE(tones.count(), expected.count());
    QCOMPARE(TonePaths::supportedSuffixes().count(), expected.count());
    for (const QString &suffix : TonePaths::supportedSuffixes())
        QVERIFY2(TonePaths::nameFilters().contains(QStringLiteral("*.") + suffix),
                 qPrintable(QStringLiteral("no discovery glob for .%1").arg(suffix)));

    cleanupTones();
}

// Import and discovery must agree on case too: a camera that writes
// "MyTone.WAV" may import it, and it must then show up in the list — never
// copy successfully and silently vanish.
void TestConfigManager::mixedCaseExtensionsImportAndAppearInDiscovery()
{
    ConfigManager cm;
    cleanupTones();
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/MyTone.WAV"), "RIFF"));
    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/Other.Mp3"), "RIFF"));
    QVERIFY(writeFile(cm.tonesDirectory() + QStringLiteral("/Dropped.FLAC"), "RIFF"));

    QCOMPARE(cm.copyToTones(
                 QUrl::fromLocalFile(sourceDir.path() + QStringLiteral("/MyTone.WAV")).toString()),
             QStringLiteral("MyTone.WAV"));
    QCOMPARE(cm.toneError(), QString());
    QCOMPARE(cm.copyToTones(
                 QUrl::fromLocalFile(sourceDir.path() + QStringLiteral("/Other.Mp3")).toString()),
             QStringLiteral("Other.Mp3"));
    QCOMPARE(cm.toneError(), QString());

    const QStringList tones = cm.availableTones();
    QVERIFY2(tones.contains(QStringLiteral("MyTone.WAV")),
             qPrintable(tones.join(QLatin1Char(','))));
    QVERIFY2(tones.contains(QStringLiteral("Other.Mp3")), qPrintable(tones.join(QLatin1Char(','))));
    QVERIFY2(tones.contains(QStringLiteral("Dropped.FLAC")),
             "a file already sitting in the tones folder must be discovered");
    QCOMPARE(tones.count(), 3);

    cleanupTones();
}

void TestConfigManager::copyToTonesImportsFromUrlWithSpacesAndUnicode()
{
    ConfigManager cm;
    cleanupTones();
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    // Space, '#', '%' and Arabic: every character the old hand-rolled
    // file:// strip got wrong, in one file name. The Arabic word is spelled
    // as UTF-8 hex here so the source file stays pure ASCII.
    const QString name = QStringLiteral("my tone #1 100% ") +
                         QString::fromUtf8(QByteArray::fromHex("d8aad986d8a8d98ad987")) +
                         QStringLiteral(".wav");
    const QString source = sourceDir.path() + QLatin1Char('/') + name;
    QVERIFY(writeFile(source, "RIFF-CONTENT"));
    QSignalSpy spy(&cm, &ConfigManager::configChanged);

    // What the FileDialog actually hands QML: a url, stringified.
    const QString asUrl = QUrl::fromLocalFile(source).toString();
    QCOMPARE(cm.copyToTones(asUrl), name);
    QCOMPARE(cm.toneError(), QString());
    QVERIFY(cm.toneErrorMessage().isEmpty());
    QCOMPARE(spy.count(), 1);

    const QString stored = cm.tonesDirectory() + QLatin1Char('/') + name;
    QVERIFY2(QFile::exists(stored), qPrintable(stored));
    QCOMPARE(readFile(stored), QByteArray("RIFF-CONTENT"));

    // A plain filesystem path keeps working for the C++ callers.
    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/plain.wav"), "x"));
    QCOMPARE(cm.copyToTones(sourceDir.path() + QStringLiteral("/plain.wav")),
             QStringLiteral("plain.wav"));
    QCOMPARE(cm.toneError(), QString());

    // Picking a file that already lives in the tones folder is a no-op that
    // still reports success — it is already in the list.
    QCOMPARE(cm.copyToTones(stored), name);
    QCOMPARE(cm.toneError(), QString());

    // The new file shows up without any manual refresh.
    QVERIFY(cm.availableTones().contains(name));
    QVERIFY(cm.availableTones().contains(QStringLiteral("plain.wav")));

    cleanupTones();
}

void TestConfigManager::copyToTonesRejectsBadInput()
{
    ConfigManager cm;
    cleanupTones();
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    // Missing source.
    QCOMPARE(cm.copyToTones(sourceDir.path() + QStringLiteral("/nope.wav")), QString());
    QCOMPARE(cm.toneError(), QStringLiteral("source-missing"));
    QVERIFY(!cm.toneErrorMessage().isEmpty());

    // Unsupported extension: rejected before anything is copied, so it can
    // never sit undiscovered in the tones folder.
    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/notes.txt"), "text"));
    QCOMPARE(cm.copyToTones(
                 QUrl::fromLocalFile(sourceDir.path() + QStringLiteral("/notes.txt")).toString()),
             QString());
    QCOMPARE(cm.toneError(), QStringLiteral("unsupported"));
    QVERIFY(!QFile::exists(cm.tonesDirectory() + QStringLiteral("/notes.txt")));

    // Empty input and a non-local url are both invalid paths.
    QCOMPARE(cm.copyToTones(QString()), QString());
    QCOMPARE(cm.toneError(), QStringLiteral("invalid-path"));
    QCOMPARE(cm.copyToTones(QStringLiteral("http://example.com/tone.wav")), QString());
    QCOMPARE(cm.toneError(), QStringLiteral("invalid-path"));

    // Every failure leaves the tones folder untouched.
    QVERIFY(cm.availableTones().isEmpty());

    // A later success clears the recorded failure.
    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/good.wav"), "RIFF"));
    QCOMPARE(cm.copyToTones(
                 QUrl::fromLocalFile(sourceDir.path() + QStringLiteral("/good.wav")).toString()),
             QStringLiteral("good.wav"));
    QCOMPARE(cm.toneError(), QString());
    QVERIFY(cm.toneErrorMessage().isEmpty());

    cleanupTones();
}

void TestConfigManager::duplicateImportAsksAndReplaceOverwrites()
{
    ConfigManager cm;
    cleanupTones();
    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    // Two different folders, same file name — the shape of a re-import.
    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/first/dupe.wav"), "FIRST"));
    QVERIFY(writeFile(sourceDir.path() + QStringLiteral("/second/dupe.wav"), "SECOND"));

    const QString first = sourceDir.path() + QStringLiteral("/first/dupe.wav");
    const QString second = sourceDir.path() + QStringLiteral("/second/dupe.wav");
    const QString stored = cm.tonesDirectory() + QStringLiteral("/dupe.wav");

    QCOMPARE(cm.copyToTones(QUrl::fromLocalFile(first).toString()), QStringLiteral("dupe.wav"));
    QCOMPARE(cm.toneError(), QString());

    // The duplicate policy is "ask": refuse, name the tone, copy nothing.
    QCOMPARE(cm.copyToTones(QUrl::fromLocalFile(second).toString()), QString());
    QCOMPARE(cm.toneError(), QStringLiteral("exists"));
    QVERIFY2(cm.toneErrorMessage().contains(QStringLiteral("dupe.wav")),
             qPrintable(cm.toneErrorMessage()));
    QCOMPARE(readFile(stored), QByteArray("FIRST"));

    // The confirmed replacement is what overwrites.
    QCOMPARE(cm.replaceTone(QUrl::fromLocalFile(second).toString()), QStringLiteral("dupe.wav"));
    QCOMPARE(cm.toneError(), QString());
    QCOMPARE(readFile(stored), QByteArray("SECOND"));

    cleanupTones();
}

void TestConfigManager::deleteToneRefusesSystemTones()
{
    ConfigManager cm;
    cleanupTones();

    const QString systemDir = systemTonesDirPath();
    QVERIFY(QDir().mkpath(systemDir));
    QVERIFY(writeFile(systemDir + QStringLiteral("/sys-tone.wav"), "RIFF"));
    QVERIFY(cm.availableTones().contains(QStringLiteral("sys-tone.wav")));

    // System tones are listed but read-only: the refusal must say why and
    // leave the packaged file alone.
    QVERIFY(!cm.deleteTone(QStringLiteral("sys-tone.wav")));
    QCOMPARE(cm.toneError(), QStringLiteral("read-only"));
    QVERIFY(!cm.toneErrorMessage().isEmpty());
    QVERIFY(QFile::exists(systemDir + QStringLiteral("/sys-tone.wav")));

    // Unknown tone, and a path that tries to escape the folder.
    QVERIFY(!cm.deleteTone(QStringLiteral("ghost.wav")));
    QCOMPARE(cm.toneError(), QStringLiteral("not-found"));
    QVERIFY(!cm.deleteTone(QStringLiteral("../settings.ini")));
    QCOMPARE(cm.toneError(), QStringLiteral("invalid-path"));
    QVERIFY(QFile::exists(settingsFilePath()));

    // A user tone still deletes, and the outcome is reported as success.
    QVERIFY(writeFile(cm.tonesDirectory() + QStringLiteral("/mine.wav"), "RIFF"));
    QVERIFY(cm.deleteTone(QStringLiteral("mine.wav")));
    QCOMPARE(cm.toneError(), QString());
    QVERIFY(!QFile::exists(cm.tonesDirectory() + QStringLiteral("/mine.wav")));

    cleanupTones();
}

void TestConfigManager::listedTonesResolveForPlayback()
{
    ConfigManager cm;
    cleanupTones();

    const QString systemDir = systemTonesDirPath();
    QVERIFY(QDir().mkpath(systemDir));
    QVERIFY(writeFile(cm.tonesDirectory() + QStringLiteral("/local.wav"), "RIFF"));
    QVERIFY(writeFile(systemDir + QStringLiteral("/packaged.wav"), "RIFF"));

    // What the alarm editor stores in soundFile is whatever availableTones()
    // hands out, so every entry must be a bare file name that resolves back to
    // a real file — otherwise selection succeeds and playback silently fails.
    const QStringList tones = cm.availableTones();
    QVERIFY(tones.contains(QStringLiteral("local.wav")));
    QVERIFY(tones.contains(QStringLiteral("packaged.wav")));
    QCOMPARE(tones.count(), 2);
    for (const QString &tone : tones) {
        QVERIFY2(!tone.contains(QLatin1Char('/')),
                 qPrintable(QStringLiteral("not a bare file name: %1").arg(tone)));
        QVERIFY2(!TonePaths::resolve(tone).isEmpty(),
                 qPrintable(QStringLiteral("listed but unresolvable: %1").arg(tone)));
    }

    QCOMPARE(TonePaths::resolve(QStringLiteral("packaged.wav")),
             systemDir + QStringLiteral("/packaged.wav"));
    QVERIFY(TonePaths::resolve(QStringLiteral("does-not-exist.wav")).isEmpty());

    cleanupTones();
}

void TestConfigManager::testWakeDurationDefaultsAndClamps()
{
    ConfigManager cm;

    // Fresh settings file: the documented default.
    QCOMPARE(cm.testWakeDuration(), 60);

    // Below the range clamps up to the minimum.
    cm.setTestWakeDuration(45);
    QCOMPARE(cm.testWakeDuration(), 60);

    // In-range value round-trips.
    cm.setTestWakeDuration(180);
    QCOMPARE(cm.testWakeDuration(), 180);

    // Above the range clamps down to the maximum.
    cm.setTestWakeDuration(99999);
    QCOMPARE(cm.testWakeDuration(), 3600);

    // The clamped value is what a fresh instance reads back from disk.
    ConfigManager reloaded;
    QCOMPARE(reloaded.testWakeDuration(), 3600);
}

void TestConfigManager::audioSettingsRoundTripAndClamp()
{
    ConfigManager cm;
    QSignalSpy spy(&cm, &ConfigManager::configChanged);

    // Fresh defaults: the guarantee policy at a 50 % floor, full stream, no
    // explicit default sound (the resolver falls back to the first tone).
    QCOMPARE(cm.audioPolicy(), QStringLiteral("guarantee"));
    QCOMPARE(cm.audioFloorPercent(), 50);
    QCOMPARE(cm.audioStreamPercent(), 100);
    QVERIFY(cm.audioDefaultSound().isEmpty());
    QVERIFY(cm.audioOutputSink().isEmpty()); // "System default" speaker

    // Policy: only the two documented values survive a write; anything else
    // reads back as the safe default instead of silently disabling the floor.
    cm.setAudioPolicy(QStringLiteral("alarmOnly"));
    QCOMPARE(cm.audioPolicy(), QStringLiteral("alarmOnly"));
    cm.setAudioPolicy(QStringLiteral("banana"));
    QCOMPARE(cm.audioPolicy(), QStringLiteral("guarantee"));

    // Percentages clamp to 0..100 on write AND on read (settings.ini may be
    // hand-edited — the controller must never see an out-of-range level).
    cm.setAudioFloorPercent(150);
    QCOMPARE(cm.audioFloorPercent(), 100);
    cm.setAudioFloorPercent(-5);
    QCOMPARE(cm.audioFloorPercent(), 0);
    cm.setAudioStreamPercent(42);
    QCOMPARE(cm.audioStreamPercent(), 42);

    // Default sound is stored trimmed (QML combos may hand over padding).
    cm.setAudioDefaultSound(QStringLiteral("  beep.wav  "));
    QCOMPARE(cm.audioDefaultSound(), QStringLiteral("beep.wav"));

    // Output device (speaker selector): trimmed node.name, "" = default.
    cm.setAudioOutputSink(QStringLiteral("  alsa_output.pci-0000_00_1b.0.analog-stereo  "));
    QCOMPARE(cm.audioOutputSink(),
             QStringLiteral("alsa_output.pci-0000_00_1b.0.analog-stereo"));

    // Every real change announces configChanged (the controller re-reads the
    // settings); a no-op write must stay silent so bindings cannot loop.
    QVERIFY(spy.count() > 0);
    const int before = spy.count();
    cm.setAudioPolicy(cm.audioPolicy());
    cm.setAudioFloorPercent(cm.audioFloorPercent());
    cm.setAudioOutputSink(cm.audioOutputSink());
    QCOMPARE(spy.count(), before);

    // Leave the shared test-mode settings at defaults for later runs.
    cm.setAudioPolicy(QStringLiteral("guarantee"));
    cm.setAudioFloorPercent(50);
    cm.setAudioStreamPercent(100);
    cm.setAudioDefaultSound(QString());
    cm.setAudioOutputSink(QString());
}

void TestConfigManager::emojiLevelDefaultsToMoreAndPersists()
{
    ConfigManager cm;
    QCOMPARE(cm.emojiLevel(), 2); // shipped default is the fuller "More" level
    QCOMPARE(cm.emoji(QStringLiteral("\U0001F514"), 1), QStringLiteral("\U0001F514 "));
    QCOMPARE(cm.emoji(QStringLiteral("\U0001F514"), 2), QStringLiteral("\U0001F514 "));

    QSignalSpy spy(&cm, &ConfigManager::configChanged);
    cm.setEmojiLevel(0);
    QCOMPARE(cm.emojiLevel(), 0);
    QVERIFY(cm.emoji(QStringLiteral("\U0001F514"), 1).isEmpty());
    QCOMPARE(spy.count(), 1);
    cm.setEmojiLevel(2);
    QCOMPARE(cm.emojiLevel(), 2);
    QCOMPARE(cm.emoji(QStringLiteral("\U0001F514"), 2), QStringLiteral("\U0001F514 "));
    QCOMPARE(spy.count(), 2);
    cm.setEmojiLevel(2);
    QCOMPARE(spy.count(), 2); // a no-op write must not re-evaluate every binding

    // Out-of-range values (a hand-edited ini) clamp on write and on read.
    cm.setEmojiLevel(9);
    QCOMPARE(cm.emojiLevel(), 2);
    cm.setEmojiLevel(-3);
    QCOMPARE(cm.emojiLevel(), 0);
    QVERIFY(cm.emoji(QStringLiteral("\U0001F514"), 1).isEmpty());
    QCOMPARE(spy.count(), 3);

    ConfigManager reloaded;
    QCOMPARE(reloaded.emojiLevel(), 0); // ui/emojiLevel survives a restart

    // The on/off key shipped in 017ee5f is migrated exactly once.
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.remove(QStringLiteral("ui/emojiLevel"));
    settings.setValue(QStringLiteral("ui/showEmojis"), true);
    settings.sync();
    ConfigManager migratedOn;
    QCOMPARE(migratedOn.emojiLevel(), 2); // old "on" maps to the new default
    settings.setValue(QStringLiteral("ui/showEmojis"), false);
    settings.sync();
    ConfigManager migratedOff;
    QCOMPARE(migratedOff.emojiLevel(), 0);

    cm.setEmojiLevel(2); // leave the shared test profile at the shipped default
    QSettings after(settingsFilePath(), QSettings::IniFormat);
    QVERIFY(!after.contains(QStringLiteral("ui/showEmojis")));
}

void TestConfigManager::emojiLevelChangeHasDedicatedSignalAndMigratesLegacyOff()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.remove(QStringLiteral("ui/emojiLevel"));
    settings.setValue(QStringLiteral("ui/showEmojis"), false);
    settings.sync();

    ConfigManager cm;
    QCOMPARE(cm.emojiLevel(), 0);
    QSignalSpy levelChanged(&cm, &ConfigManager::emojiLevelChanged);

    // Selecting the same effective level used to remove the only key and let
    // the next start fall back to More. It must persist Off instead.
    cm.setEmojiLevel(0);
    QSettings after(settingsFilePath(), QSettings::IniFormat);
    QVERIFY(after.contains(QStringLiteral("ui/emojiLevel")));
    QCOMPARE(after.value(QStringLiteral("ui/emojiLevel")).toInt(), 0);
    QVERIFY(!after.contains(QStringLiteral("ui/showEmojis")));
    ConfigManager reloaded;
    QCOMPARE(reloaded.emojiLevel(), 0);
    QCOMPARE(levelChanged.count(), 0); // no effective change -> no UI refresh

    cm.setEmojiLevel(1);
    QCOMPARE(levelChanged.count(), 1);
    QCOMPARE(levelChanged.at(0).at(0).toInt(), 1);
    cm.setEmojiLevel(2); // leave the shared profile at the default
    QCOMPARE(levelChanged.count(), 2);
}

void TestConfigManager::cleanupTones()
{
    const QString tonesDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
                             QStringLiteral("/lumalarm/tones");
    QDir dir(tonesDir);
    if (dir.exists())
        dir.removeRecursively();
    QDir().mkpath(tonesDir);

    // The temp XDG data dir is shared by every system-tone case; drop it too
    // so one test can never observe another one's leftovers.
    QDir system(systemTonesDirPath());
    if (system.exists())
        system.removeRecursively();
}

QTEST_GUILESS_MAIN(TestConfigManager)
#include "tst_configmanager.moc"
