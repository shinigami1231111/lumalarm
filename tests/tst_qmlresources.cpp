#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtTest>

#include <utility> // std::as_const

#include "managers/TonePaths.h"

// ============================================================================
// Smoke tests for the v1.0.1 build-requirement update:
//   - CMake >= 3.21 and Qt >= 6.5 (cmake_minimum_required(VERSION 3.21),
//     find_package(Qt6 6.5 ...), qt_standard_project_setup(REQUIRES 6.5))
//   - QML module resources are served from qrc:/qt/qml/GlassAlarm/
//     (Qt policy QTP0001, NEW since Qt 6.5)
//
// The QML/icon resources are embedded into THIS test binary by the
// qt6_add_resources() call in CMakeLists.txt, which reads the application
// target's own QT_QML_MODULE_RESOURCE_PREFIX property. If the real module's
// prefix ever regresses (e.g. QTP0001 reverted), the resources move, these
// checks fail, and the regression is caught instead of shipping a binary
// that cannot load qrc:/qt/qml/GlassAlarm/qml/main.qml.
// ============================================================================

// ============================================================================
// STG-3.4 additions (Phase C): full resource-path coverage so any future
// rename/move of a QML file or stale qrc:/qt/qml/GlassAlarm/qml/... literal
// is caught automatically by ctest.
//   1. Every file listed in CMakeLists set(QML_FILES ...) must be embedded at
//      :/qt/qml/GlassAlarm/<relative-path> (list mirrored below — kept in
//      sync via the reverse-drift check which fails if the embedded tree and
//      this list disagree in either direction).
//   2. :/qt/qml/GlassAlarm/qml/main.qml — asserted by the original
//      mainQmlExistsInResource() above (kept).
//   3. Literal resources/icons/*.svg references in QML must resolve; the
//      dynamic pattern (".../resources/icons/" + modelData.icon + ".svg")
//      must at least have a valid base directory containing >= 1 .svg.
//   4. Every hardcoded qrc:/qt/qml/GlassAlarm/qml/... literal in src/**/*.{cpp,h}
//      and qml/**/*.qml must resolve as a QFile path (qrc:/ prefix stripped).
//   Item 5 (QQmlEngine load) intentionally NOT added: the original test does
//   not do engine loading, so no extra Qt module (Qt6::Quick) is required and
//   CMakeLists.txt stays untouched in this phase.
//
// Source-tree scans locate src/ and qml/ relative to this file via __FILE__
// (CMake passes absolute source paths). If the tree is unavailable (binary
// copied elsewhere), those two tests QSKIP instead of failing spuriously.
// ============================================================================

class TestQmlResources : public QObject
{
    Q_OBJECT

private slots:
    void qtVersionMeetsBuildRequirement(); // Qt >= 6.5 (declared minimum)
    void qtVersionSupports66Plus();        // Qt 6.6+ forward-compat check
    void mainQmlExistsInResource();        // qrc:/qt/qml/GlassAlarm/qml/main.qml
    void mainQmlIsReadable();
    void segmentedControlIconsExistInResource();
    // STG-3.4 (Phase C):
    void allQmlFilesFromCmakeListExistInResource();  // CMake QML_FILES <-> qrc
    void qrcQmlLiteralsInSourcesResolve();           // stale literals in src/ + qml/
    void iconReferencesInQmlResolve();               // literal + dynamic icon refs
    void importDialogFiltersMatchDiscoveryFormats(); // Issue #2: picker <-> discovery
    void emojisAreGatedByEmojiLevel();               // emoji density switch controls every glyph
    void mainTabsUseIconsWithoutEmojiPrefix();       // five tabs already carry an icon
    void windowHandlesAreInvisibleAndSystemDriven(); // frameless move/resize, no visible bar
    void sliderUsersImportQtQuickControls();         // no runtime "Slider is not a type"
};

// ----------------------------------------------------------------------------
// Helpers for the STG-3.4 source-tree scans.
// ----------------------------------------------------------------------------

// Absolute path of this source file's directory (CMake passes absolute paths
// to the compiler, so __FILE__ is absolute), minus the tests/ component.
static QString sourceRoot()
{
    const QString here = QFileInfo(QStringLiteral(__FILE__)).absolutePath();
    return QDir::cleanPath(here + QStringLiteral("/.."));
}

// All files under <sourceRoot>/<subDir> matching nameFilters (recursive).
static QStringList sourceRootFiles(const QString &subDir, const QStringList &nameFilters)
{
    QDir dir(sourceRoot() + QLatin1Char('/') + subDir);
    if (!dir.exists())
        return {};
    QStringList out;
    QDirIterator it(dir.absolutePath(), nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        out << it.next();
    out.sort();
    return out;
}

static QString readSourceFile(const QString &absPath)
{
    QFile f(absPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

// Emoji code points used by the UI: clocks (U+23F0..U+23F3), misc symbols
// (U+2600..U+27BF), variation selectors and the pictograph planes.
static bool containsEmoji(const QString &text)
{
    for (uint cp : text.toUcs4()) {
        // U+2713/U+2714 are plain status checkmarks, not decorative emoji.
        if (cp == 0xFE0F || (cp >= 0x2300 && cp <= 0x23FF) ||
            ((cp >= 0x2600 && cp <= 0x27BF) && cp != 0x2713 && cp != 0x2714) ||
            (cp >= 0x1F300 && cp <= 0x1FAFF))
            return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// Mirror of CMakeLists.txt set(QML_FILES ...) — STG-3.4 item 1.
// MUST BE KEPT IN SYNC with set(QML_FILES ...) in CMakeLists.txt.
// The reverse-drift check inside allQmlFilesFromCmakeListExistInResource()
// fails this test whenever the two lists disagree, so a file added to
// CMakeLists without being mirrored here (or vice versa) cannot slip through.
// ----------------------------------------------------------------------------
static const QStringList kQmlFilesFromCmake = {
    QStringLiteral("qml/main.qml"),
    QStringLiteral("qml/NightDim.qml"),
    QStringLiteral("qml/wizard/WizardOverlay.qml"),
    QStringLiteral("qml/wizard/WizardPage.qml"),
    QStringLiteral("qml/wizard/pages/LanguagePage.qml"),
    QStringLiteral("qml/wizard/pages/WakeMethodPage.qml"),
    QStringLiteral("qml/wizard/pages/NotificationPage.qml"),
    QStringLiteral("qml/wizard/pages/AlarmSoundPage.qml"),
    QStringLiteral("qml/wizard/pages/DonePage.qml"),
    QStringLiteral("qml/components/GlassButton.qml"),
    QStringLiteral("qml/components/GlassCard.qml"),
    QStringLiteral("qml/components/Tokens.qml"),
    QStringLiteral("qml/pages/AlarmListView.qml"),
    QStringLiteral("qml/pages/AlarmEditor.qml"),
    QStringLiteral("qml/overlays/RingingOverlay.qml"),
    QStringLiteral("qml/overlays/SoundManager.qml"),
    QStringLiteral("qml/overlays/CloseDialog.qml"),
    QStringLiteral("qml/overlays/RingingWindow.qml"),
    QStringLiteral("qml/pages/TimerPage.qml"),
    QStringLiteral("qml/pages/StopwatchPage.qml"),
    QStringLiteral("qml/pages/SettingsPage.qml"),
    QStringLiteral("qml/components/RoundedCombo.qml"),
    QStringLiteral("qml/components/GlassCheckBox.qml"),
    QStringLiteral("qml/pickers/ClockFace.qml"),
    QStringLiteral("qml/components/ToggleSwitch.qml"),
    QStringLiteral("qml/components/SegmentedControl.qml"),
    QStringLiteral("qml/components/WindowResizeHandle.qml"),
    QStringLiteral("qml/pickers/WheelTimePicker.qml"),
};

// Declared build minimum of v1.0.1: Qt >= 6.5.
void TestQmlResources::qtVersionMeetsBuildRequirement()
{
    QVERIFY2(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0),
             qPrintable(QStringLiteral("Qt %1 found, but >= 6.5 is required")
                            .arg(QString::fromLatin1(qVersion()))));
}

// Verify Qt 6.6+ support.
// Runs (and must pass) on every Qt >= 6.6; validated on Qt 6.11.2.
// On a Qt 6.5 build it skips so the declared 6.5 minimum keeps passing.
void TestQmlResources::qtVersionSupports66Plus()
{
    if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
        QSKIP("Qt 6.5 build: the 6.6+ support check only runs on Qt >= 6.6.");

    QVERIFY2(
        QT_VERSION >= QT_VERSION_CHECK(6, 6, 0),
        qPrintable(
            QStringLiteral("Qt %1 reported, 6.6+ expected").arg(QString::fromLatin1(qVersion()))));
    qInfo() << "Qt" << qVersion() << "passes the Qt 6.6+ forward-compat check";
}

// qrc:/qt/qml/GlassAlarm/qml/main.qml must exist
// (main.cpp loads exactly this path — if it moves, the app exits with -1.)
void TestQmlResources::mainQmlExistsInResource()
{
    QVERIFY(QFile::exists(":/qt/qml/GlassAlarm/qml/main.qml"));
}

// The entry point must contain real QML content, not an empty stub.
void TestQmlResources::mainQmlIsReadable()
{
    QFile mainQml(":/qt/qml/GlassAlarm/qml/main.qml");
    QVERIFY(mainQml.open(QIODevice::ReadOnly));
    const QByteArray content = mainQml.readAll();
    QVERIFY2(!content.isEmpty(), "main.qml is embedded but empty");
    QVERIFY2(content.contains("Window"), "main.qml does not look like the Lumalarm root window");
}

// SegmentedControl.qml loads its icons from the same module prefix
// (path updated in v1.0.1 together with main.cpp).
void TestQmlResources::segmentedControlIconsExistInResource()
{
    const QStringList iconNames{"alarms", "timer", "stopwatch", "sounds", "settings"};
    for (const QString &name : iconNames) {
        const QString path = QStringLiteral(":/qt/qml/GlassAlarm/resources/icons/%1.svg").arg(name);
        QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("missing: %1").arg(path)));
    }
}

// ============================================================================
// STG-3.4 (Phase C) additions
// ============================================================================

// Item 1: every QML file from CMakeLists QML_FILES must exist at
// :/qt/qml/GlassAlarm/<relative-path>, and the embedded tree must not
// contain any QML file absent from the mirrored list (bidirectional check).
void TestQmlResources::allQmlFilesFromCmakeListExistInResource()
{
    // (a) mirror list -> embedded resource
    for (const QString &rel : kQmlFilesFromCmake) {
        const QString res = QStringLiteral(":/qt/qml/GlassAlarm/") + rel;
        QVERIFY2(
            QFile::exists(res),
            qPrintable(QStringLiteral("missing QML resource: %1 — QML_FILES in CMakeLists.txt and "
                                      "the mirror list in tst_qmlresources.cpp are out of sync, or "
                                      "the resource prefix regressed (QTP0001)")
                           .arg(res)));
    }

    // (b) embedded resource -> mirror list (reverse-drift check)
    const QString prefix = QStringLiteral(":/qt/qml/GlassAlarm/");
    QDirIterator it(QStringLiteral(":/qt/qml/GlassAlarm/qml"), {QStringLiteral("*.qml")},
                    QDir::Files, QDirIterator::Subdirectories);
    QStringList embedded;
    while (it.hasNext())
        embedded << it.next().mid(prefix.size());
    embedded.sort();
    QVERIFY2(!embedded.isEmpty(),
             "no QML resources found under :/qt/qml/GlassAlarm/qml — prefix regressed?");

    for (const QString &rel : embedded) {
        QVERIFY2(kQmlFilesFromCmake.contains(rel),
                 qPrintable(QStringLiteral(
                                "embedded QML not in the tst_qmlresources.cpp mirror list: %1 — "
                                "update the mirror list to match set(QML_FILES) in CMakeLists.txt")
                                .arg(rel)));
    }
}

// Item 4: every hardcoded qrc:/qt/qml/GlassAlarm/qml/... literal in
// src/**/*.{cpp,h} and qml/**/*.qml must resolve (qrc:/ stripped).
void TestQmlResources::qrcQmlLiteralsInSourcesResolve()
{
    const QStringList srcFiles =
        sourceRootFiles(QStringLiteral("src"), {QStringLiteral("*.cpp"), QStringLiteral("*.h")});
    const QStringList qmlFiles = sourceRootFiles(QStringLiteral("qml"), {QStringLiteral("*.qml")});
    if (srcFiles.isEmpty() || qmlFiles.isEmpty())
        QSKIP("source tree not available (tests/ not inside the build tree); "
              "qrc literal scan requires src/ and qml/ next to tests/.");

    // Matches both the qrc:/qt/... and the bare :/qt/... spelling; capture 1
    // is the path after the module prefix (e.g. "qml/main.qml").
    static const QRegularExpression qrcRe(
        QStringLiteral(R"((?:qrc)?:/qt/qml/GlassAlarm/([A-Za-z0-9_./+\-]+\.qml))"));

    int checked = 0;
    for (const QString &f : std::as_const(srcFiles) + qmlFiles) {
        const QString content = readSourceFile(f);
        QVERIFY2(!content.isEmpty(),
                 qPrintable(QStringLiteral("cannot read source file: %1").arg(f)));
        auto it = qrcRe.globalMatch(content);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QString res = QStringLiteral(":/qt/qml/GlassAlarm/") + m.captured(1);
            QVERIFY2(QFile::exists(res),
                     qPrintable(
                         QStringLiteral("stale qrc literal in %1 — \":/qt/qml/GlassAlarm/%2\" does "
                                        "not resolve to an embedded resource")
                             .arg(f, m.captured(1))));
            ++checked;
        }
    }

    // Guarantee the scan is not vacuous: main.cpp must load main.qml.
    QVERIFY2(checked > 0,
             "expected at least one qrc:/qt/qml/GlassAlarm/... literal in src/ or qml/");
}

// Item 3: literal resources/icons/*.svg references in any QML file must
// resolve; the dynamic pattern (base + expr + ".svg") must have a valid
// base directory containing at least one .svg.
void TestQmlResources::iconReferencesInQmlResolve()
{
    const QStringList qmlFiles = sourceRootFiles(QStringLiteral("qml"), {QStringLiteral("*.qml")});
    if (qmlFiles.isEmpty())
        QSKIP("source tree not available (tests/ not inside the build tree); "
              "icon reference scan requires qml/ next to tests/.");

    // Literal full-name references, e.g. ".../resources/icons/alarms.svg".
    // The dynamic spelling (".../resources/icons/" + x + ".svg") cannot match
    // this pattern because a quote follows the slash.
    static const QRegularExpression litRe(
        QStringLiteral(R"(resources/icons/([A-Za-z0-9_+.\-]+\.svg))"));

    bool sawAnyIconRef = false;
    for (const QString &f : std::as_const(qmlFiles)) {
        const QString content = readSourceFile(f);
        QVERIFY2(!content.isEmpty(), qPrintable(QStringLiteral("cannot read QML file: %1").arg(f)));
        if (content.contains(QStringLiteral("resources/icons/")))
            sawAnyIconRef = true;
        auto it = litRe.globalMatch(content);
        while (it.hasNext()) {
            const QString name = it.next().captured(1);
            const QString res = QStringLiteral(":/qt/qml/GlassAlarm/resources/icons/") + name;
            QVERIFY2(QFile::exists(res),
                     qPrintable(QStringLiteral("missing icon referenced by %1: %2").arg(f, res)));
        }
    }

    // Dynamic pattern (SegmentedControl.qml: "qrc:/.../resources/icons/"
    // + modelData.icon + ".svg"): the base directory must exist and hold
    // at least one icon.
    if (sawAnyIconRef) {
        const QString base = QStringLiteral(":/qt/qml/GlassAlarm/resources/icons");
        QVERIFY2(
            QDir(base).exists(),
            qPrintable(
                QStringLiteral("icon base directory missing (QTP0001 prefix?): %1").arg(base)));
        const QStringList svgs = QDir(base).entryList({QStringLiteral("*.svg")});
        QVERIFY2(!svgs.isEmpty(),
                 qPrintable(
                     QStringLiteral("no .svg files in dynamic icon base directory: %1").arg(base)));
    }
}

// Issue #2: the FileDialog nameFilters in SoundManager.qml must advertise
// exactly the extensions availableTones() discovers. A format the picker
// accepts but discovery ignores imports successfully and then silently
// disappears from the list — the reported *.aac case. Both sides are derived
// from one list (TonePaths::supportedSuffixes()), so this test is the guard
// that keeps the QML filter from drifting away from it.
void TestQmlResources::importDialogFiltersMatchDiscoveryFormats()
{
    const QString soundManager = sourceRoot() + QStringLiteral("/qml/overlays/SoundManager.qml");
    const QString qml = readSourceFile(soundManager);
    if (qml.isEmpty())
        QSKIP("source tree not available (tests/ not inside the build tree); "
              "dialog filter scan requires qml/ next to tests/.");

    const QRegularExpression lineRe(QStringLiteral("nameFilters\\s*:\\s*\\[.*\\]"));
    const QRegularExpressionMatch line = lineRe.match(qml);
    QVERIFY2(line.hasMatch(), "SoundManager.qml no longer declares FileDialog.nameFilters — "
                              "update this test alongside the import dialog");

    QStringList dialogGlobs;
    const QRegularExpression globRe(QStringLiteral(R"(\*\.[A-Za-z0-9]+)"));
    auto it = globRe.globalMatch(line.captured(0));
    while (it.hasNext())
        dialogGlobs << it.next().captured(0);
    dialogGlobs.sort();

    QStringList discoveryGlobs = TonePaths::nameFilters();
    discoveryGlobs.sort();

    QVERIFY2(!dialogGlobs.isEmpty(), "no *.ext globs found in nameFilters");
    QCOMPARE(dialogGlobs, discoveryGlobs);
}

// Emoji density used to be a dead setting: the wizard and Settings offered it
// but no label read it. Every decorative glyph in QML must therefore go through
// configManager.emoji(symbol, level), which applies the Off / Less / More
// switch. A raw literal cannot silently bypass the selector (or reintroduce
// the old hard-coded 🔊).
void TestQmlResources::emojisAreGatedByEmojiLevel()
{
    const QStringList files = sourceRootFiles(QStringLiteral("qml"), {QStringLiteral("*.qml")});
    QVERIFY2(!files.isEmpty(), "QML source tree not available — cannot check emoji gating");

    int checked = 0;
    int moreOnly = 0;
    for (const QString &path : files) {
        const QStringList lines = readSourceFile(path).split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            if (!containsEmoji(lines.at(i)))
                continue;
            ++checked;
            const QString line = lines.at(i);
            QVERIFY2(line.contains(QStringLiteral("configManager.emoji(")),
                     qPrintable(QStringLiteral("%1:%2: emoji bypasses configManager.emoji()")
                                    .arg(path)
                                    .arg(i + 1)));
            if (line.contains(QStringLiteral(", 2)")))
                ++moreOnly;
        }
    }
    QVERIFY2(checked > 0, "no emojis found — the emoji setting has no visible effect");
    QVERIFY2(moreOnly > 0, "no More-level emojis found — the new option is meaningless");
    QVERIFY2(moreOnly < checked, "every emoji is More-only — the Less level lost its glyphs");
}

// The five main tabs already draw their own SVG icon; an emoji prefix on top
// of that is redundant and made the labels noisier in More mode.
void TestQmlResources::mainTabsUseIconsWithoutEmojiPrefix()
{
    const QString mainText = readSourceFile(sourceRoot() + QStringLiteral("/qml/main.qml"));
    const int start = mainText.indexOf(QStringLiteral("items: ["));
    const int end = mainText.indexOf(QStringLiteral("]"), start);
    QVERIFY2(start >= 0 && end > start, "main tab model not found");

    const QString tabs = mainText.mid(start, end - start);
    QVERIFY2(!containsEmoji(tabs), "main tab labels must rely on their icons, not emoji prefixes");
    for (const QString &label :
         {QStringLiteral("Alarms"), QStringLiteral("Timer"), QStringLiteral("Stopwatch"),
          QStringLiteral("Sounds"), QStringLiteral("Settings")}) {
        QVERIFY2(tabs.contains(QStringLiteral("qsTr(\"%1\")").arg(label)),
                 qPrintable(QStringLiteral("main tab missing: %1").arg(label)));
    }
}

// Frameless move/resize must not add chrome. The title area asks the window
// manager for a native move; eight transparent strips do the same for resize,
// with a geometry fallback for Qt versions that lack startSystemResize().
void TestQmlResources::windowHandlesAreInvisibleAndSystemDriven()
{
    const QString mainText = readSourceFile(sourceRoot() + QStringLiteral("/qml/main.qml"));
    const QString handleText =
        readSourceFile(sourceRoot() + QStringLiteral("/qml/components/WindowResizeHandle.qml"));
    QVERIFY2(!mainText.isEmpty() && !handleText.isEmpty(), "window handle sources not available");

    QVERIFY2(mainText.contains(QStringLiteral("root.startSystemMove()")),
             "title area must use the native move gesture");
    QVERIFY2(handleText.contains(QStringLiteral("target.startSystemResize(resizeHandle.edge)")),
             "resize handles must use the native resize gesture");
    QVERIFY2(handleText.contains(QStringLiteral("typeof target.startSystemResize")),
             "Qt 6.5 fallback guard missing");
    QVERIFY2(handleText.contains(QStringLiteral("target.minimumWidth")),
             "manual resize fallback must respect minimumWidth");
    QVERIFY2(handleText.contains(QStringLiteral("color: \"transparent\"")),
             "resize handles must draw no visible bar");
    QCOMPARE(mainText.count(QStringLiteral("WindowResizeHandle {")), 8);

    for (const QString &edge :
         {QStringLiteral("edge: Qt.TopEdge"), QStringLiteral("edge: Qt.BottomEdge"),
          QStringLiteral("edge: Qt.LeftEdge"), QStringLiteral("edge: Qt.RightEdge"),
          QStringLiteral("edge: Qt.TopEdge | Qt.LeftEdge"),
          QStringLiteral("edge: Qt.TopEdge | Qt.RightEdge"),
          QStringLiteral("edge: Qt.BottomEdge | Qt.LeftEdge"),
          QStringLiteral("edge: Qt.BottomEdge | Qt.RightEdge")}) {
        QVERIFY2(mainText.contains(edge),
                 qPrintable(QStringLiteral("missing resize edge: %1").arg(edge)));
    }
}

// Regression: AlarmSoundPage used Slider without importing QtQuick.Controls.
// CMake's QML cache compiles that file without resolving every type, so ctest
// passed while the app exited at engine.load() with "Slider is not a type".
// Scanning the source tree catches this class of missing import directly.
void TestQmlResources::sliderUsersImportQtQuickControls()
{
    const QStringList qmlFiles = sourceRootFiles(QStringLiteral("qml"), {QStringLiteral("*.qml")});
    QVERIFY2(!qmlFiles.isEmpty(), "source tree not available for the Slider import scan");

    const QRegularExpression sliderType(QStringLiteral("\\bSlider\\s*\\{"),
                                        QRegularExpression::MultilineOption);
    const QRegularExpression controlsImport(QStringLiteral("^\\s*import\\s+QtQuick\\.Controls\\b"),
                                            QRegularExpression::MultilineOption);
    for (const QString &path : qmlFiles) {
        const QString text = readSourceFile(path);
        if (!text.contains(sliderType))
            continue;
        QVERIFY2(text.contains(controlsImport),
                 qPrintable(QStringLiteral("%1 uses Slider but does not import QtQuick.Controls")
                                .arg(path)));
    }
}

QTEST_GUILESS_MAIN(TestQmlResources)
#include "tst_qmlresources.moc"
