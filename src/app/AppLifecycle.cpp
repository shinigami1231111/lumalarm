#include "app/AppLifecycle.h"

#include "audio/AlarmAudioController.h"
#include "managers/ConfigManager.h"
#include "managers/ThemeManager.h"
#include "ui/BlurManager.h"
#include "ui/TrayController.h"

#include <QTimer>
#include <QtLogging>

AppLifecycle::AppLifecycle(ConfigManager *cfg, BlurManager *blur, QObject *parent)
    : QObject(parent), m_cfg(cfg), m_blur(blur)
{
}

void AppLifecycle::attachWindow(QQuickWindow *w)
{
    m_window = w;
    if (!m_window)
        return;

    // `this` is the context, so these die with us — never a dangling capture (I3).
    connect(m_window, &QWindow::visibleChanged, this, [this](bool) {
        emit windowShownChanged();
        emit canHideToTrayChanged();
    });
    connect(m_window, &QWindow::visibilityChanged, this, [this](QWindow::Visibility) {
        emit windowShownChanged();
    });

    // Blur region: the native surface is destroyed on hide, so show() re-issues
    // it (above). A resize also invalidates the region — recompute here, again
    // with `this` as context and no captures but `this` (I3, I7).
    auto reblur = [this]() { applyNativeHooks(); };
    connect(m_window, &QWindow::xChanged, this, reblur);
    connect(m_window, &QWindow::yChanged, this, reblur);
    connect(m_window, &QWindow::widthChanged, this, reblur);
    connect(m_window, &QWindow::heightChanged, this, reblur);

    // Theme changes can flip blur_mode; re-apply against the current window.
    if (m_cfg && m_cfg->theme())
        connect(m_cfg->theme(), &ThemeManager::themeChanged, this, reblur);

    // Apply once now so a window that was created already-visible gets blur.
    applyNativeHooks();
}

void AppLifecycle::attachTray(TrayController *t)
{
    m_tray = t;
    if (!m_tray)
        return;

    // Queued (I4): the tray's activated() arrives on the D-Bus/menu callback
    // stack. Never raise a window from inside that stack — do it on the next
    // trip through the event loop.
    connect(m_tray, &TrayController::showRequested, this,
            &AppLifecycle::showMainWindow, Qt::QueuedConnection);
    connect(m_tray, &TrayController::quitRequested, this, &AppLifecycle::quit,
            Qt::QueuedConnection);
    connect(m_tray, &TrayController::usableChanged, this,
            &AppLifecycle::canHideToTrayChanged);

    // The tray icon follows the setting. The connection context is `this`, so it
    // is torn down with us; the old code captured a stack local by [&] instead.
    auto sync = [this]() {
        if (m_tray)
            m_tray->setEnabled(m_cfg && m_cfg->trayIconEnabled());
    };
    sync();
    if (m_cfg)
        connect(m_cfg, &ConfigManager::configChanged, this, sync);
}

bool AppLifecycle::windowShown() const
{
    return m_window && m_window->isVisible() && m_window->visibility() != QWindow::Minimized;
}

bool AppLifecycle::canHideToTray() const
{
    // I8: a hide needs a tray the user can actually click back through.
    return m_state == Running && m_tray && m_tray->usable() && m_cfg
        && m_cfg->trayIconEnabled();
}

// I7. KWin blur-behind is a request against a NATIVE window. Hiding destroys the
// native surface, so the request dies with it and must be re-issued on every
// show. Nothing here caches a winId()/QPlatformWindow pointer.
void AppLifecycle::applyNativeHooks()
{
    if (!m_window || !m_blur)
        return;
    const bool compositor = m_cfg && m_cfg->theme()
        && m_cfg->theme()->blur_mode() == QLatin1String("compositor");
    m_blur->setBlurEnabled(m_window, compositor);
}

void AppLifecycle::showMainWindow()
{
    if (m_state != Running || !m_window)
        return;

    // Exactly one show call (I1). setVisible(true)+showNormal() back to back
    // from a D-Bus callback was one of the suspects — that pattern is gone.
    if (m_window->visibility() == QWindow::Hidden)
        m_window->show();
    else if (m_window->visibility() == QWindow::Minimized)
        m_window->showNormal();

    // The native surface may have just been recreated — re-ask for blur (I7).
    applyNativeHooks();

    m_window->raise();
    // Best effort. Wayland may refuse focus; that is not an error.
    m_window->requestActivate();
}

void AppLifecycle::hideMainWindow()
{
    hideOrMinimize();
}

void AppLifecycle::hideOrMinimize()
{
    if (m_state != Running || !m_window)
        return;
    if (canHideToTray())
        m_window->hide();
    else
        // I8: never leave the app invisible with no way back.
        m_window->showMinimized();
}

void AppLifecycle::requestClose()
{
    if (m_state != Running)
        return;                                                    // I5

    const QString a = m_cfg ? m_cfg->closeAction() : QStringLiteral("ask");
    if (a == QLatin1String("quit"))
        quit();
    else if (a == QLatin1String("hide"))
        hideOrMinimize();
    else
        emit closePromptRequested();
}

void AppLifecycle::confirmClose(const QString &action, bool remember)
{
    if (m_state != Running)
        return;
    if (action != QLatin1String("hide") && action != QLatin1String("quit"))
        return;

    // Only commit something we can honour: remembering "hide" with no tray
    // would strand the user on every future close.
    if (remember && m_cfg && (action == QLatin1String("quit") || canHideToTray()))
        m_cfg->setCloseAction(action);

    if (action == QLatin1String("quit"))
        quit();
    else
        hideOrMinimize();
}

void AppLifecycle::quit()
{
    if (m_state == Quitting)
        return;                                                    // I5 idempotent
    shutdownOnce();
    // Never quit from inside a signal handler: post it.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void AppLifecycle::attachAudio(AlarmAudioController *a)
{
    m_audio = a;
}

void AppLifecycle::shutdownOnce()
{
    if (m_state == Quitting)
        return;
    m_state = Quitting;
    emit quittingChanged();

    // Audio restore FIRST: the sink must be back exactly before anything
    // tears down (guide). shutdownForQuit() is synchronous and bounded
    // (~800 ms); on timeout the write-ahead journal survives and the next
    // start's recover() finishes the restore.
    if (m_audio)
        m_audio->shutdownForQuit();

    // Explicit, synchronous, before the event loop ends (I6). Left to
    // destructors, the tray can still deliver a click while the engine is
    // tearing the window down.
    if (m_tray)
        m_tray->shutdown();
}

void AppLifecycle::start(bool startHidden)
{
    m_state = Running;
    emit windowShownChanged();
    emit canHideToTrayChanged();

    // --hidden for autostart. If the tray is not usable yet the window is shown
    // rather than lost (I8).
    if (startHidden && canHideToTray())
        return;
    showMainWindow();
}
