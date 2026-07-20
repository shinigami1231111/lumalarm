#include "BlurManager.h"

#if defined(LUMALARM_ENABLE_KDE_BLUR)
#include <KWindowEffects>
#include <KWindowInfo>
#endif

BlurManager::BlurManager(QObject *parent)
    : QObject(parent)
{
}

void BlurManager::setBlurEnabled(QQuickWindow *window, bool enable)
{
#if defined(LUMALARM_ENABLE_KDE_BLUR)
    if (!window)
        return;

    m_enabled = enable;
    if (enable) {
        // Blur behind the whole window. KWin ignores opaque regions itself,
        // and blurring the full window is the most reliable approach.
        KWindowEffects::enableBlurBehind(window, true);
    } else {
        // Explicitly remove the blur-behind request.
        KWindowEffects::enableBlurBehind(window, false);
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(enable);
#endif
}

void BlurManager::updateRegion(QQuickWindow *window)
{
#if defined(LUMALARM_ENABLE_KDE_BLUR)
    if (!window)
        return;
    if (m_enabled)
        KWindowEffects::enableBlurBehind(window, true);
#else
    Q_UNUSED(window);
#endif
}
