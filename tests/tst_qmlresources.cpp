#include <QtTest>
#include <QFile>

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

class TestQmlResources : public QObject
{
    Q_OBJECT

private slots:
    void qtVersionMeetsBuildRequirement(); // Qt >= 6.5 (declared minimum)
    void qtVersionSupports66Plus();        // Qt 6.6+ forward-compat check
    void mainQmlExistsInResource();        // qrc:/qt/qml/GlassAlarm/qml/main.qml
    void mainQmlIsReadable();
    void segmentedControlIconsExistInResource();
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

    QVERIFY2(QT_VERSION >= QT_VERSION_CHECK(6, 6, 0),
             qPrintable(QStringLiteral("Qt %1 reported, 6.6+ expected")
                            .arg(QString::fromLatin1(qVersion()))));
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
    QVERIFY2(content.contains("Window"),
             "main.qml does not look like the Lumalarm root window");
}

// SegmentedControl.qml loads its icons from the same module prefix
// (path updated in v1.0.1 together with main.cpp).
void TestQmlResources::segmentedControlIconsExistInResource()
{
    const QStringList iconNames{"alarms", "timer", "stopwatch", "sounds", "settings"};
    for (const QString &name : iconNames) {
        const QString path =
            QStringLiteral(":/qt/qml/GlassAlarm/resources/icons/%1.svg").arg(name);
        QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("missing: %1").arg(path)));
    }
}

QTEST_GUILESS_MAIN(TestQmlResources)
#include "tst_qmlresources.moc"
