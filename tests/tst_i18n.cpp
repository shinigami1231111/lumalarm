#include <QCoreApplication>
#include <QFile>
#include <QList>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QStringList>
#include <QTranslator>
#include <QtTest>

#include "managers/ConfigManager.h"

// ============================================================================
// Unit tests for the i18n layer (STG-5.2).
//
// What is covered here:
//   * the English source strings survive with no translator installed, which
//     is the state a machine without Qt Linguist ends up in;
//   * the compiled Arabic and Japanese catalogs are embedded at the exact
//     resource path main.cpp loads (":/i18n/<name>.qm") and really translate
//     strings — including placeholders and leading/trailing spaces, which the
//     C++ side does .arg() on and concatenates;
//   * a live QML binding that used qsTr() is re-evaluated by
//     QQmlEngine::retranslate() after a translator is installed *and* after it
//     is removed, i.e. the round trip the language selector performs;
//   * ui/language round-trips through ConfigManager and only signals on a
//     real change;
//   * the committed .ts catalogs stay in sync — a new string pulled by
//     lupdate but never translated fails the test instead of silently
//     falling back to English in the UI.
//
// The catalog assertions are property-based (translated, Arabic script,
// placeholder intact, whitespace intact) rather than exact-string matches, so
// rewording a translation never turns the test into a translation lock-in.
//
// QStandardPaths test mode is enabled in initTestCase() so nothing here
// touches the real ~/.config/lumalarm.
//
// The catalogs are only embedded when Qt LinguistTools was available at
// configure time; those cases report a skip rather than a false failure.
// ============================================================================

namespace {

// True when the string contains at least one Arabic-script character
// (U+0600..U+06FF). Used to prove a lookup really came back translated
// instead of merely differing from the source.
bool containsArabic(const QString &text)
{
    for (const QChar &ch : text) {
        const char16_t u = ch.unicode();
        if (u >= 0x0600 && u <= 0x06FF)
            return true;
    }
    return false;
}

// True when the string contains at least one kana character (U+3040..U+30FF).
bool containsJapanese(const QString &text)
{
    for (const QChar &ch : text) {
        const char16_t u = ch.unicode();
        if (u >= 0x3040 && u <= 0x30FF)
            return true;
    }
    return false;
}

// Answers in memory so the retranslate() path is exercised even on a build
// without compiled catalogs. Everything else returns a null QString, which
// Qt treats as "I have no translation for this".
class FakeTranslator : public QTranslator
{
public:
    using QTranslator::QTranslator;

    QString translate(const char *context, const char *sourceText, const char *disambiguation,
                      int n) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)
        if (qstrcmp(sourceText, "Settings") == 0)
            return QStringLiteral("PARAMS-MARK");
        return QString();
    }
};

int countOf(const QByteArray &haystack, const char *needle)
{
    int count = 0;
    int at = haystack.indexOf(needle);
    while (at >= 0) {
        ++count;
        at = haystack.indexOf(needle, at + 1);
    }
    return count;
}

} // namespace

class TstI18n : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void sourceFallbackWithoutTranslator();
    void arabicCatalogIsEmbedded();
    void arabicCatalogTranslatesKnownStrings();
    void japaneseCatalogIsEmbedded();
    void japaneseCatalogTranslatesKnownStrings();
    void retranslateUpdatesLiveQmlBinding();
    void languageSettingRoundTrips();
    void committedCatalogsAreConsistent();
};

void TstI18n::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TstI18n::sourceFallbackWithoutTranslator()
{
    // No translator is installed in this process, so every lookup must come
    // back byte-identical to the source. This is what a machine without
    // Qt Linguist (and the CI image) actually renders.
    QCOMPARE(QCoreApplication::translate("SettingsPage", "Settings"), QStringLiteral("Settings"));
    QCOMPARE(QCoreApplication::translate("WizardController", "Wake test failed: %1"),
             QStringLiteral("Wake test failed: %1"));
}

void TstI18n::arabicCatalogIsEmbedded()
{
    QFile catalog(QStringLiteral(":/i18n/lumalarm_ar.qm"));
    if (!catalog.open(QIODevice::ReadOnly))
        QSKIP("Arabic catalog not embedded (Qt LinguistTools unavailable at configure time)");
    QVERIFY2(catalog.size() > 1000, "embedded Arabic catalog is suspiciously small");
}

void TstI18n::arabicCatalogTranslatesKnownStrings()
{
    QFile catalog(QStringLiteral(":/i18n/lumalarm_ar.qm"));
    if (!catalog.open(QIODevice::ReadOnly))
        QSKIP("Arabic catalog not embedded (Qt LinguistTools unavailable at configure time)");

    QTranslator translator;
    QVERIFY2(translator.load(QStringLiteral(":/i18n/lumalarm_ar.qm")),
             "QTranslator refused the embedded catalog");
    QCoreApplication::installTranslator(&translator);

    // A plain QML string (context = QML file base name) comes back translated.
    const QString settings = QCoreApplication::translate("SettingsPage", "Settings");
    QVERIFY2(settings != QStringLiteral("Settings"), "SettingsPage/Settings is untranslated");
    QVERIFY2(containsArabic(settings), "SettingsPage/Settings is not Arabic");

    // A plain C++ string (context = QObject subclass name).
    const QString quit = QCoreApplication::translate("TrayController", "Quit");
    QVERIFY2(quit != QStringLiteral("Quit"), "TrayController/Quit is untranslated");
    QVERIFY2(containsArabic(quit), "TrayController/Quit is not Arabic");

    // The placeholder must survive .arg() exactly as authored, otherwise the
    // runtime message loses the reason/duration it is supposed to report.
    const QString failed = QCoreApplication::translate("WizardController", "Wake test failed: %1")
                               .arg(QStringLiteral("boom"));
    QVERIFY2(failed.contains(QStringLiteral("boom")), "translation dropped the %1 placeholder");
    QVERIFY2(containsArabic(failed), "Wake test failed is not Arabic");

    // Leading/trailing spaces are significant: the UI concatenates these
    // fragments around a countdown, so trimming them corrupts the output.
    const QString hourSuffix = QCoreApplication::translate("main", "h ");
    QVERIFY2(hourSuffix.endsWith(QLatin1Char(' ')), "trailing space dropped from 'h '");
    QVERIFY2(hourSuffix != QStringLiteral("h "), "'h ' was left untranslated");

    const QString minutePrefix = QCoreApplication::translate("SettingsPage", " min");
    QVERIFY2(minutePrefix.startsWith(QLatin1Char(' ')), "leading space dropped from ' min'");
    QVERIFY2(minutePrefix != QStringLiteral(" min"), "' min' was left untranslated");

    // Unknown text still falls back to the source instead of an empty label.
    QCOMPARE(QCoreApplication::translate("SettingsPage", "No such string exists here"),
             QStringLiteral("No such string exists here"));

    QCoreApplication::removeTranslator(&translator);
}

void TstI18n::japaneseCatalogIsEmbedded()
{
    QFile catalog(QStringLiteral(":/i18n/lumalarm_ja.qm"));
    if (!catalog.open(QIODevice::ReadOnly))
        QSKIP("Japanese catalog not embedded (Qt LinguistTools unavailable at configure time)");
    QVERIFY2(catalog.size() > 1000, "embedded Japanese catalog is suspiciously small");
}

void TstI18n::japaneseCatalogTranslatesKnownStrings()
{
    QFile catalog(QStringLiteral(":/i18n/lumalarm_ja.qm"));
    if (!catalog.open(QIODevice::ReadOnly))
        QSKIP("Japanese catalog not embedded (Qt LinguistTools unavailable at configure time)");

    // Load exactly as main.cpp does: the selector writes "ja" and the app asks
    // QTranslator for that locale, not for a hardcoded file name.
    QTranslator translator;
    const QLocale japanese(QStringLiteral("ja"));
    QVERIFY2(translator.load(japanese, QStringLiteral("lumalarm"), QStringLiteral("_"),
                             QStringLiteral(":/i18n")),
             "QTranslator could not resolve the 'ja' selector value to a catalog");
    // "System default" resolves to a full locale name such as ja_JP.
    QTranslator systemLocale;
    QVERIFY2(systemLocale.load(QLocale(QStringLiteral("ja_JP")), QStringLiteral("lumalarm"),
                               QStringLiteral("_"), QStringLiteral(":/i18n")),
             "ja_JP did not fall back to the ja catalog");
    QCoreApplication::installTranslator(&translator);

    // A C++ string with a placeholder and a QML string (context = file base name)
    // must both come from the catalog, not the English source.
    const QString failed = QCoreApplication::translate("WizardController", "Wake test failed: %1")
                               .arg(QStringLiteral("boom"));
    QVERIFY2(containsJapanese(failed), "Wake test failed is not Japanese");
    QVERIFY2(failed.contains(QStringLiteral("boom")), "translation dropped the %1 placeholder");

    const QString language = QCoreApplication::translate("LanguagePage", "Show emojis");
    QVERIFY2(containsJapanese(language), "LanguagePage/Show emojis is not Japanese");
    QVERIFY2(language != QStringLiteral("Show emojis"), "LanguagePage/Show emojis is untranslated");

    // Whitespace around these fragments is significant: C++ concatenates them
    // with a number in between, so trimming would corrupt the countdown.
    const QString hourSuffix = QCoreApplication::translate("main", "h ");
    QVERIFY2(hourSuffix.endsWith(QLatin1Char(' ')), "trailing space dropped from 'h '");
    QVERIFY2(hourSuffix != QStringLiteral("h "), "'h ' was left untranslated");

    const QString minutePrefix = QCoreApplication::translate("SettingsPage", " min");
    QVERIFY2(minutePrefix.startsWith(QLatin1Char(' ')), "leading space dropped from ' min'");
    QVERIFY2(minutePrefix != QStringLiteral(" min"), "' min' was left untranslated");

    QCOMPARE(QCoreApplication::translate("SettingsPage", "No such string exists here"),
             QStringLiteral("No such string exists here"));

    QCoreApplication::removeTranslator(&translator);
}

void TstI18n::retranslateUpdatesLiveQmlBinding()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml\n"
                      "QtObject {\n"
                      "    property string label: qsTr(\"Settings\")\n"
                      "}\n",
                      QUrl(QStringLiteral("qrc:/tst_i18n_i18n_probe.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    QCOMPARE(object->property("label").toString(), QStringLiteral("Settings"));

    // Install a translator on the application — exactly what main.cpp does
    // when the language selector changes — and tell the engine to re-read.
    FakeTranslator fake;
    QCoreApplication::installTranslator(&fake);
    engine.retranslate();
    QCOMPARE(object->property("label").toString(), QStringLiteral("PARAMS-MARK"));

    // ...and back again, so switching to a language with no catalog leaves
    // English source text on screen rather than a stale translation.
    QCoreApplication::removeTranslator(&fake);
    engine.retranslate();
    QCOMPARE(object->property("label").toString(), QStringLiteral("Settings"));
}

void TstI18n::languageSettingRoundTrips()
{
    ConfigManager config;
    // The QStandardPaths test-mode settings file is shared with the other
    // test binaries and may already hold a value, so seed a known state
    // instead of assuming a pristine profile.
    config.setLanguage(QString());
    QVERIFY(config.language().isEmpty()); // "" == follow the system locale

    config.setLanguage(QStringLiteral("ar"));
    QCOMPARE(config.language(), QStringLiteral("ar"));
    // The wizard and Settings write "ja" — the key must survive unchanged even
    // though Arabic is no longer offered in the UI.
    config.setLanguage(QStringLiteral("ja"));
    QCOMPARE(config.language(), QStringLiteral("ja"));
    config.setLanguage(QStringLiteral("ar")); // restore the value used below

    QSignalSpy changed(&config, &ConfigManager::languageChanged);
    config.setLanguage(QStringLiteral("ar")); // no change -> no signal
    QCOMPARE(changed.count(), 0);
    config.setLanguage(QStringLiteral("en"));
    QCOMPARE(changed.count(), 1);

    // A second instance reads the persisted value back from disk.
    ConfigManager reloaded;
    QCOMPARE(reloaded.language(), QStringLiteral("en"));

    // Leave the shared profile on the default so this test cannot leak a
    // language choice into the other test binaries.
    config.setLanguage(QString());
}

void TstI18n::committedCatalogsAreConsistent()
{
    const QString dir = QString::fromUtf8(LUMALARM_SOURCE_DIR) + QStringLiteral("/translations/");
    const QStringList catalogs{QStringLiteral("lumalarm_en.ts"), QStringLiteral("lumalarm_ar.ts"),
                               QStringLiteral("lumalarm_ja.ts")};

    QList<int> counts;
    for (const QString &name : catalogs) {
        QFile file(dir + name);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
        const QByteArray xml = file.readAll();
        const int messages = countOf(xml, "<message>");
        QVERIFY2(messages > 0, "lupdate extracted no strings — check the qsTr()/tr() wrappers");
        counts << messages;

        if (name == QStringLiteral("lumalarm_en.ts"))
            continue; // the source catalog deliberately has empty translations

        // lupdate marks brand-new strings type="unfinished". An unfinished
        // entry means the string ships untranslated, so the UI silently falls
        // back to English for it. Fail the test instead.
        QVERIFY2(
            !xml.contains("type=\"unfinished\""),
            qPrintable(name + QStringLiteral(" has untranslated entries — run "
                                             "`make update_translations` and translate them")));
        QVERIFY2(!xml.contains("type=\"vanished\""),
                 qPrintable(name + QStringLiteral(" has obsolete entries")));
    }

    // All three catalogs track the same source strings; lupdate keeps them in
    // step, but a hand-edited or stale catalog shows up here.
    QCOMPARE(counts.at(0), counts.at(1));
    QCOMPARE(counts.at(0), counts.at(2));
}

QTEST_GUILESS_MAIN(TstI18n)
#include "tst_i18n.moc"
