#include "ui/NightMode.h"

#include "managers/ConfigManager.h"
#include "wake/Scheduler.h"

#include <QDebug>

NightMode::NightMode(ConfigManager *config, Scheduler *scheduler, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_scheduler(scheduler)
{
    // Dependencies are passed explicitly; never rediscovered through parent().
    if (!m_config || !m_scheduler)
        qWarning() << "[NightMode] constructed without config and/or scheduler —"
                      "night mode stays inactive";

    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(kPreviewMillis);
    connect(&m_previewTimer, &QTimer::timeout, this, &NightMode::deactivate);
}

bool NightMode::enabled() const
{
    return m_config && m_config->nightModeEnabled();
}

void NightMode::setEnabled(bool enabled)
{
    if (!m_config || m_config->nightModeEnabled() == enabled)
        return;
    m_config->setNightModeEnabled(enabled);
    emit configReloaded();
    // Turning the feature off must drop the overlay right away.
    evaluate();
}

int NightMode::minutesBefore() const
{
    return m_config ? m_config->nightModeMinutesBefore() : 30;
}

void NightMode::setMinutesBefore(int minutes)
{
    if (!m_config || m_config->nightModeMinutesBefore() == minutes)
        return;
    m_config->setNightModeMinutesBefore(minutes);
    emit configReloaded();
    // The activation instant moved: re-evaluate so the overlay follows.
    evaluate();
}

double NightMode::dimOpacity() const
{
    return m_config ? m_config->nightModeOpacity() : 0.4;
}

void NightMode::setDimOpacity(double opacity)
{
    if (!m_config)
        return;
    m_config->setNightModeOpacity(opacity);
    emit configReloaded();
}

bool NightMode::active() const
{
    return m_active;
}

void NightMode::activate()
{
    m_previewTimer.start();
    setActive(true);
}

void NightMode::deactivate()
{
    m_previewTimer.stop();
    setActive(false);
}

void NightMode::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged(m_active);
}

void NightMode::evaluate()
{
    if (!m_config || !m_scheduler)
        return;

    // A manual preview owns the overlay for its own duration; the 1 Hz tick
    // must not switch it off under the user's finger.
    if (m_previewTimer.isActive())
        return;

    // Feature off, or nothing scheduled → no overlay.
    if (!enabled()) {
        setActive(false);
        return;
    }
    const int secondsUntil = m_scheduler->secondsUntilNextAlarm();
    if (secondsUntil <= 0) {
        setActive(false);
        return;
    }

    const int leadSeconds = minutesBefore() * 60;
    const bool inside = secondsUntil <= leadSeconds;
    // Edge trigger: only a flip of the predicate changes anything, so a long
    // stay inside the window emits exactly one signal.
    setActive(inside);
}
