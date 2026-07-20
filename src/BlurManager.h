#ifndef BLURMANAGER_H
#define BLURMANAGER_H

#include <QObject>
#include <QQuickWindow>

// KDE-native blur-behind support. Under KWin, transparent windows do NOT
// get blurred automatically (unlike Hyprland windowrules). We must ask KWin
// explicitly via KWindowEffects::enableBlurBehind, and re-issue the request
// on resize and when the blur mode changes.
//
// All KWindowSystem usage is guarded by LUMALARM_ENABLE_KDE_BLUR so the app
// still builds/runs on systems without KDE libraries installed — the calls
// simply become no-ops there.
class BlurManager : public QObject
{
    Q_OBJECT
public:
    explicit BlurManager(QObject *parent = nullptr);

    // Enable or disable KWin blur-behind for the given window.
    // enable = true  -> request blur (only in compositor blur mode)
    // enable = false -> remove the blur-behind request
    // A null/!isValid window is ignored.
    Q_INVOKABLE void setBlurEnabled(QQuickWindow *window, bool enable);

    // Re-apply the blur region after a resize.
    Q_INVOKABLE void updateRegion(QQuickWindow *window);

#if defined(LUMALARM_ENABLE_KDE_BLUR)
private:
    QRegion m_region;
    bool m_enabled = false;
#endif
};

#endif // BLURMANAGER_H
