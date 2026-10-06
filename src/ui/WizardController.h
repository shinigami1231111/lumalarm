#ifndef WIZARDCONTROLLER_H
#define WIZARDCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

class ConfigManager;
class WakeManager;
class AudioPlayer;

// First-run wizard (STG-5.5): a six-step guided setup that gets a brand-new
// user to a working state — language chosen, wake method verified, notification
// capability probed, alarm sound previewed — without reading the README.
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
    // UI language chosen on page 2. Read straight from ConfigManager — there is
    // exactly one language setting (STG-5.2), the wizard only writes to it.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    // Status line of the wake test page ("Wake test succeeded" / "...failed: why").
    Q_PROPERTY(QString wakeStatus READ wakeStatus NOTIFY wakeStatusChanged)
    // Chosen tone for page 2 / the summary row on page 6. Notifying, so the
    // summary stays correct after the user changes it on an earlier page.
    Q_PROPERTY(
        QString defaultSound READ defaultSound WRITE setDefaultSound NOTIFY defaultSoundChanged)
    // Session-bus probe for org.freedesktop.Notifications (non-blocking, cached
    // per call site — QML re-reads it whenever the page becomes visible).
    Q_PROPERTY(
        bool notificationsAvailable READ notificationsAvailable NOTIFY notificationProbeChanged)

public:
    // Three non-owning dependencies; all MUST be non-null and outlive this
    // object (they are stack locals in main(), declared before it).
    explicit WizardController(ConfigManager *config, WakeManager *wake, AudioPlayer *audio,
                              QObject *parent = nullptr);

    int currentStep() const;
    int stepCount() const;
    bool canGoBack() const;
    // Gate: the wake page only unlocks Next after a successful probe or an
    // explicit "Skip this step".
    bool canGoNext() const;
    bool wizardCompleted() const;
    QString language() const;
    QString wakeStatus() const;
    bool notificationsAvailable() const;

    // How many steps the wizard has (kStepCount). QML reads it, never hardcodes.
    static constexpr int kStepCount = 6;

    // WakeManager::armTestWake() accepts only [60, 3600] s, so the shortest
    // non-destructive probe is 60 s — a 5 s request would be refused outright.
    // Mirrors WakeManager::kTestWakeMinSeconds, which is private, and the
    // constant lives here rather than in WakeManager because STG-5.5 must not
    // change that class.
    static constexpr int kWakeProbeSeconds = 60;

    // Step indices, so the QML and the tests agree on what "step 1" means.
    // Language first (STG-5.5.2), then sound, then the test alarm, then the
    // notification probe, then the summary.
    enum Step { Welcome = 0, Language = 1, Sound = 2, WakeMethod = 3, Notifications = 4, Done = 5 };

    Q_INVOKABLE void next();
    Q_INVOKABLE void back();
    // Skip: mark completed (so it never auto-shows again) and hide. It writes
    // nothing else — the language persisted itself the moment it was picked
    // (retranslation is live), while the sound and the wake probe are never
    // persisted at all.
    Q_INVOKABLE void skip();
    // Finish: mark completed and hide. Idempotent by the same guard.
    Q_INVOKABLE void finish();
    // Page 6 secondary link: close the wizard for this run WITHOUT writing
    // wizard/completed, so the next launch shows it again.
    Q_INVOKABLE void finishRepeatNextTime();
    // Settings > "Run setup wizard again": clear the flag, rewind to step 0
    // and make the overlay reappear.
    Q_INVOKABLE void reset();

    // Page 2: the language selector. Writes ConfigManager's single ui/language
    // key; main.cpp already swaps the catalog and retranslates on change, so
    // Arabic takes effect (and mirrors the layout) immediately.
    void setLanguage(const QString &lang);

    // Page 3: arm the shortest wake WakeManager accepts, report the outcome,
    // then cancel it immediately — non-destructive by construction.
    Q_INVOKABLE bool testWake();
    // Page 3: explicit "Skip this step" that unlocks Next.
    Q_INVOKABLE void skipWakeStep();
    // Page 4: send one benign notification; false when there is no daemon.
    Q_INVOKABLE bool testNotification();

    // Pages 3 and 6: plain-language description of the wake mechanism
    // WakeManager reports for this machine.
    Q_INVOKABLE QString detectWakeMethod() const;

    // Page 2: tones already shipped by the app (ConfigManager::availableTones).
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
    void languageChanged();
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
