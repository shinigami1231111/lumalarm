#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    void defaultWakeModeRoundTripsThroughSettings();
    void timePickerModeIsNormalizedOnRead();
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
