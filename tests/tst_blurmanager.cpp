#include <QtTest>
#include <QQuickWindow>

#include "ui/BlurManager.h"

// ============================================================================
// Unit tests for BlurManager (src/ui/). The KDE blur-behind calls are guarded
// by LUMALARM_ENABLE_KDE_BLUR and every entry point documents that a null
// window is ignored — that guard is what is testable in a headless GUILESS run.
//
// Not covered: an actual blur request against a live KWin window. Follow-up:
// refactor BlurManager to take an injectable KWindowEffects backend so the
// enable/disable requests can be asserted without a compositor session.
// ============================================================================

class TestBlurManager : public QObject
{
    Q_OBJECT

private slots:
    void constructionIsSafe();
    void nullWindowIsIgnoredByEveryEntryPoint();
    void blurRequestNeedsLiveKWinWindow();
};

void TestBlurManager::constructionIsSafe()
{
    // No KWin/KWindowSystem state is touched by the constructor, so merely
    // constructing it must be safe in a headless test run.
    BlurManager manager;

    QCOMPARE(manager.parent(), nullptr);
    QVERIFY(manager.metaObject() != nullptr);
}

void TestBlurManager::nullWindowIsIgnoredByEveryEntryPoint()
{
    BlurManager manager;

    // Documented contract: a null window is ignored on both entry points,
    // with and without the KDE backend compiled in.
    manager.setBlurEnabled(nullptr, true);
    manager.setBlurEnabled(nullptr, false);
    manager.updateRegion(nullptr);
    QVERIFY(true); // reaching here without a crash is the assertion

    // The QML-facing entry points must stay registered in the meta-object.
    QVERIFY(manager.metaObject()->indexOfMethod("setBlurEnabled(QQuickWindow*,bool)") >= 0);
    QVERIFY(manager.metaObject()->indexOfMethod("updateRegion(QQuickWindow*)") >= 0);
}

void TestBlurManager::blurRequestNeedsLiveKWinWindow()
{
    QSKIP("requires a live QQuickWindow in a KWin/compositor session — follow-up: "
          "inject a KWindowEffects backend so enable/disable can be asserted headless");
}

QTEST_GUILESS_MAIN(TestBlurManager)
#include "tst_blurmanager.moc"
