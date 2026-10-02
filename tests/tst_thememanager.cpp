#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QFile>

#include "managers/ThemeManager.h"

// ============================================================================
// Unit tests for ThemeManager (src/managers/). Covers the seeded defaults of
// theme.conf, the built-in palette presets (applyPreset), and the write/read
// round-trip through the theme.conf file by constructing a second manager over
// the same (test-mode) config directory.
//
// QStandardPaths test mode is enabled in initTestCase() so nothing here
// touches the real ~/.config/lumalarm/theme.conf.
// ============================================================================

class TestThemeManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void seededDefaultsMatchDocumentedPalette();
    void applyPresetChangesAndPersistsPalette();
    void applyPresetRejectsUnknownName();
    void everyListedPresetApplies();

private:
    void removeThemeFile();
};

void TestThemeManager::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    removeThemeFile(); // start from a seeded-from-defaults state
}

void TestThemeManager::seededDefaultsMatchDocumentedPalette()
{
    ThemeManager tm;

    QVERIFY(tm.configPath().endsWith(QStringLiteral("theme.conf")));
    QVERIFY2(QFile::exists(tm.configPath()),
             qPrintable(QStringLiteral("theme.conf not created: %1").arg(tm.configPath())));

    QCOMPARE(tm.background_color(), QStringLiteral("#0d0d1a"));
    QCOMPARE(tm.accent_color(), QStringLiteral("#3d7fff"));
    QCOMPARE(tm.text_primary(), QStringLiteral("#ffffff"));
    QCOMPARE(tm.blur_mode(), QStringLiteral("compositor"));
    QCOMPARE(tm.corner_radius(), 22);
    // No preset has been applied yet, so the preset name is empty by design.
    QCOMPARE(tm.current_preset(), QString());
}

void TestThemeManager::applyPresetChangesAndPersistsPalette()
{
    ThemeManager tm;
    QSignalSpy spy(&tm, &ThemeManager::themeChanged);

    QVERIFY(tm.applyPreset(QStringLiteral("Nord")));
    QCOMPARE(tm.background_color(), QStringLiteral("#2e3440"));
    QCOMPARE(tm.accent_color(), QStringLiteral("#88c0d0"));
    QCOMPARE(tm.current_preset(), QStringLiteral("Nord"));
    QVERIFY(spy.count() >= 1);

    // A second manager over the same file must load the applied palette back.
    ThemeManager reloaded;
    QCOMPARE(reloaded.background_color(), QStringLiteral("#2e3440"));
    QCOMPARE(reloaded.current_preset(), QStringLiteral("Nord"));
}

void TestThemeManager::applyPresetRejectsUnknownName()
{
    ThemeManager tm;
    removeThemeFile();
    ThemeManager fresh; // re-seeded with defaults

    const QString before = fresh.background_color();
    QVERIFY(!fresh.applyPreset(QStringLiteral("no-such-preset")));
    // An unknown name is a no-op: colors and preset name stay untouched.
    QCOMPARE(fresh.background_color(), before);
    QCOMPARE(fresh.current_preset(), QString());
}

void TestThemeManager::everyListedPresetApplies()
{
    ThemeManager tm;
    const QStringList names = tm.presetNames();
    QVERIFY2(!names.isEmpty(), "presetNames() returned nothing");

    for (const QString &name : names) {
        QVERIFY2(tm.applyPreset(name),
                 qPrintable(QStringLiteral("preset failed to apply: %1").arg(name)));
        QCOMPARE(tm.current_preset(), name);
    }
}

void TestThemeManager::removeThemeFile()
{
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/lumalarm/theme.conf");
    QFile::remove(path);
}

QTEST_GUILESS_MAIN(TestThemeManager)
#include "tst_thememanager.moc"
