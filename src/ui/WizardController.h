#ifndef WIZARDCONTROLLER_H
#define WIZARDCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

class ConfigManager;
class WakeManager;
class AudioPlayer;

// First-run wizard (STG-5.5): a five-step guided setup that gets a brand-new
// user to a working state — wake method verified, notification capability
// probed, alarm sound previewed — without reading the README.
//
// Dependencies are explicit and non-owning (same contract as NightMode);
// nothing is ever looked up through parent(). All interaction with the QML
// layer happens through the `wizard` context property.
//
// State discipline mirrors NightMode::setActive(): every transition goes
// through a private setter that returns early when nothing actually changed,
// so a signal is emitted only on a real change (finish() twice must not emit
// wizardCompletedChanged twice).
class WizardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)
    Q_PROPERTY(int stepCount READ stepCount CONSTANT)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY currentStepChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY currentStepChanged)
    Q_PROPERTY(bool wizardCompleted READ wizardCompleted NOTIFY wizardCompletedChanged)
    // Status line of the wake test page ("Wake test succeeded" / "...failed: why").
    Q_PROPERTY(QString wakeStatus READ wakeStatus NOTIFY wakeStatusChanged)
    // Chosen tone for page 4 / the summary row on page 5. Notifying, so the
    // summary stays correct after the user changes it on an earlier page.
    Q_PROPERTY(QString defaultSound READ defaultSound WRITE setDefaultSound NOTIFY defaultSoundChanged)
    // Session-bus probe for org.freedesktop.Notifications (non-blocking, cached
    // per call site — QML re-reads it whenever the page becomes visible).
    Q_PROPERTY(bool notificationsAvailable READ notificationsAvailable NOTIFY notificationProbeChanged)

public:
    // Three non-owning dependencies; all MUST be non-null and outlive this
    // object (they are stack locals in main(), declared before it).
    explicit WizardController(ConfigManager *config,
                              WakeManager *wake,
                              AudioPlayer *audio,
                              QObject *parent = nullptr);

    int currentStep() const;
    int stepCount() const;
    bool canGoBack() const;
    // Gate: the wake page only unlocks Next after a successful probe or an
    // explicit "Skip this step".
    bool canGoNext() const;
    bool wizardCompleted() const;
    QString wakeStatus() const;
    bool notificationsAvailable() const;

    // How many steps the wizard has (kStepCount). QML reads it, never hardcodes.
    static constexpr int kStepCount = 5;

    // WakeManager::armTestWake() accepts only [60, 3600] s, so the shortest
    // non-destructive probe is 60 s — a 5 s request would be refused outright.
    // Mirrors WakeManager::kTestWakeMinSeconds, which is private, and the
    // constant lives here rather than in WakeManager because STG-5.5 must not
    // change that class.
    static constexpr int kWakeProbeSeconds = 60;

    // Step indices, so the QML and the tests agree on what "step 1" means.
    enum Step { Welcome = 0, WakeMethod = 1, Notifications = 2, Sound = 3, Done = 4 };

    Q_INVOKABLE void next();
    Q_INVOKABLE void back();
    // Skip: mark completed (so it never auto-shows again) and hide. Deliberately
    // does NOT apply anything chosen on the pages walked so far.
    Q_INVOKABLE void skip();
    // Finish: mark completed and hide. Idempotent by the same guard.
    Q_INVOKABLE void finish();
    // Page 5 secondary link: close the wizard for this run WITHOUT writing
    // wizard/completed, so the next launch shows it again.
    Q_INVOKABLE void finishRepeatNextTime();
    // Settings > "Run setup wizard again": clear the flag, rewind to step 0
    // and make the overlay reappear.
    Q_INVOKABLE void reset();

    // Page 2: arm the shortest wake WakeManager accepts, report the outcome,
    // then cancel it immediately — non-destructive by construction.
    Q_INVOKABLE bool testWake();
    // Page 2: explicit "Skip this step" that unlocks Next.
    Q_INVOKABLE void skipWakeStep();
    // Page 3: send one benign notification; false when there is no daemon.
    Q_INVOKABLE bool testNotification();

    // Pages 2 and 5: plain-language description of the wake mechanism
    // WakeManager reports for this machine.
    Q_INVOKABLE QString detectWakeMethod() const;

    // Page 4: tones already shipped by the app (ConfigManager::availableTones).
    // Never a hard-coded list.
    Q_INVOKABLE QVariantList availableSounds() const;
    Q_INVOKABLE void previewSound(const QString &id);
    Q_INVOKABLE void setDefaultSound(const QString &id);
    Q_INVOKABLE QString defaultSound() const;

    // --show-wizard: force the wizard visible for this run regardless of the
    // persisted flag, without rewriting it.
    void setForceShow(bool force);

Q_SIGNALS:
    void currentStepChanged();
    void wizardCompletedChanged();
    void wakeStatusChanged();
    void notificationProbeChanged();
    void defaultSoundChanged();

protected:
    // Seam for the privileged, helper-dependent half of testWake(). The real
    // implementation calls WakeManager::armTestWake(); the unit test overrides
    // it to supply both outcomes without a D-Bus helper / polkit.
    virtual bool armProbeWake();

private:
    void setCurrentStep(int step);
    void setCompleted(bool completed);
    void probeNotifications();

    ConfigManager *m_config;
    WakeManager *m_wake;
    AudioPlayer *m_audio;
    int m_currentStep = 0;
    bool m_completed = false;
    bool m_forceShow = false;
    bool m_wakeProbePassed = false;
    bool m_wakeProbeSkipped = false;
    QString m_wakeStatus;
    QString m_defaultSound;
    bool m_notificationsAvailable = false;
};

#endif // WIZARDCONTROLLER_H
