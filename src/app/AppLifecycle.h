#ifndef APPLIFECYCLE_H
#define APPLIFECYCLE_H

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QString>

class AlarmAudioController;
class BlurManager;
class ConfigManager;
class TrayController;

// The single owner of the main window's visibility and of quitting.
//
// WHY THIS EXISTS. Phase 0 proved the hide->show crash was a
// stack-use-after-scope: main.cpp declared `auto applyBlur = [&]{...}` inside
// an `if` block and connected a `[&]` lambda to QWindow::visibleChanged with no
// context object. `applyBlur` died when the block ended; every later show called
// the dead lambda, which read a garbage `configManager` and crashed. Two more
// `[&]` captures (showWindowFromTray -> mainWin) had the same shape.
//
// This class exists so that class of bug cannot recur:
//   I1  Nothing but AppLifecycle touches the window's visibility or state.
//   I2  QPointer, never a raw pointer, to anything the QML engine owns.
//   I3  Every connect() has `this` as context; no [&] lambdas anywhere.
//   I4  Tray -> lifecycle signals are queued, never acted on in the D-Bus stack.
//   I5  quit() is idempotent; once it starts every other entry point is a no-op.
//   I6  Tray teardown is explicit, synchronous and happens before the loop ends.
//   I7  Native window handles are never cached across hide/show; the KWin blur
//       hook is re-applied on every show instead.
//   I8  Never invisible-and-unrecoverable: hide needs a usable tray, else minimize.
//   I9  X, WM close, Alt+F4 and the tray all funnel into requestClose()/quit().
class AppLifecycle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool windowShown READ windowShown NOTIFY windowShownChanged)
    Q_PROPERTY(bool canHideToTray READ canHideToTray NOTIFY canHideToTrayChanged)
    Q_PROPERTY(bool quitting READ quitting NOTIFY quittingChanged)

public:
    enum State { Starting, Running, Quitting };
    Q_ENUM(State)

    AppLifecycle(ConfigManager *cfg, BlurManager *blur, QObject *parent = nullptr);

    // QML never assigns `visible`; it only calls these.
    Q_INVOKABLE void requestClose();                                    // I9
    Q_INVOKABLE void confirmClose(const QString &action, bool remember);
    Q_INVOKABLE void showMainWindow();
    Q_INVOKABLE void hideMainWindow();                                  // used by stress hooks
    Q_INVOKABLE void quit();

    void attachWindow(QQuickWindow *w);
    void attachTray(TrayController *t);
    // Audio restore hook: quit() must restore the sink BEFORE teardown
    // (guide: synchronous, bounded). Stack-owned by main(), QPointer like
    // everything else engine/main owns (I2).
    void attachAudio(AlarmAudioController *a);
    void start(bool startHidden);

    bool windowShown() const;        // visible AND not minimized
    bool canHideToTray() const;
    bool quitting() const { return m_state == Quitting; }

Q_SIGNALS:
    void closePromptRequested();
    void windowShownChanged();
    void canHideToTrayChanged();
    void quittingChanged();

private:
    void hideOrMinimize();
    void shutdownOnce();
    void applyNativeHooks();        // I7 — re-applied on every show, never cached

    State m_state = Starting;
    QPointer<QQuickWindow> m_window;
    QPointer<TrayController> m_tray;
    QPointer<AlarmAudioController> m_audio;
    QPointer<ConfigManager> m_cfg;
    BlurManager *m_blur = nullptr;  // owned by main(), outlives the engine
};

#endif // APPLIFECYCLE_H
