#ifndef NIGHTMODE_H
#define NIGHTMODE_H

#include <QObject>
#include <QTimer>

class ConfigManager;
class Scheduler;

// Night mode: dims the window with a translucent overlay a configurable number
// of minutes before the next alarm, and restores it when the alarm fires or is
// cancelled / disabled / deleted.
//
// No system backlight is touched: no root, no extra dependency, works on any
// compositor. The overlay itself lives in QML (qml/NightDim.qml) and simply
// binds to active() / dimOpacity().
//
// Activation is EDGE-triggered: evaluate() only emits when the "is the next
// alarm inside the lead window" predicate flips, so one crossing produces exactly
// one activeChanged() signal no matter how often evaluate() runs (the Scheduler
// ticks once per second).
class NightMode : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configReloaded)
    Q_PROPERTY(int minutesBefore READ minutesBefore WRITE setMinutesBefore NOTIFY configReloaded)
    Q_PROPERTY(double dimOpacity READ dimOpacity WRITE setDimOpacity NOTIFY configReloaded)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    // Both pointers are non-owning and MUST be non-null; they outlive this
    // object because both are stack locals declared before it in main().
    explicit NightMode(ConfigManager *config, Scheduler *scheduler, QObject *parent = nullptr);

    // Settings, mirrored from ConfigManager (persisted in settings.ini).
    bool enabled() const;
    void setEnabled(bool enabled);
    int minutesBefore() const;
    void setMinutesBefore(int minutes);
    double dimOpacity() const;
    void setDimOpacity(double opacity);

    // True while the overlay should be visible.
    bool active() const;

    // Manual control (used by the settings preview and by QML).
    // activate() holds the overlay for kPreviewMillis, after which deactivate()
    // runs on its own; automatic evaluation is suspended meanwhile so the 1 Hz
    // scheduler tick cannot cancel the preview one second in.
    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();

    // Re-evaluates the lead window. Connected to Scheduler::nextAlarmChanged
    // (1 Hz) and to every settings change. Idempotent.
    Q_INVOKABLE void evaluate();

    // How long a manual activate() preview stays on.
    static constexpr int kPreviewMillis = 2000;

Q_SIGNALS:
    void activeChanged(bool active);
    void configReloaded();

private:
    // Single place where m_active may change: never emits a duplicate.
    void setActive(bool active);

    ConfigManager *m_config;
    Scheduler *m_scheduler;
    QTimer m_previewTimer;
    bool m_active = false;
};

#endif // NIGHTMODE_H
