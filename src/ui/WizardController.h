#ifndef WIZARDCONTROLLER_H
#define WIZARDCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

class ConfigManager;
class WakeManager;
class AlarmAudioController;

// First-run wizard (STG-5.5): a five-step guided setup — language chosen,
// wake method confirmed, notification capability probed, alarm sound and
// output device picked — without reading the README.
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
    // UI language chosen on page 1. Read straight from ConfigManager — there is
    // exactly one language setting (STG-5.2), the wizard only writes to it.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    // Status line of the wake page ("Wake test succeeded" / "...failed: why").
    Q_PROPERTY(QString wakeStatus READ wakeStatus NOTIFY wakeStatusChanged)
    // Session-bus probe for org.freedesktop.Notifications (non-blocking, cached
    // per call site — QML re-reads it whenever the page becomes visible).
    Q_PROPERTY(
        bool notificationsAvailable READ notificationsAvailable NOTIFY notificationProbeChanged)
    // Output devices reported by the audio backend, for the alarm-sound page.
    // Mirrors AlarmAudioController::outputDevices so the wizard can refresh
    // through the same controller it uses for every other step.
    Q_PROPERTY(QVariantList outputDevices READ outputDevices NOTIFY outputDevicesChanged)
    // Display options persisted with the language.
    Q_PROPERTY(bool mirrorApp READ mirrorApp WRITE setMirrorApp NOTIFY configChangedMirror)
    Q_PROPERTY(
        bool mirrorWindowButtons READ mirrorWindowButtons WRITE setMirrorWindowButtons
            NOTIFY configChangedMirror)
    Q_PROPERTY(int emojiLevel READ emojiLevel WRITE setEmojiLevel NOTIFY configChangedMirror)

public:
    // Three non-owning dependencies; all MUST be non-null and outlive this
    // object (they are stack locals in main(), declared before it).
    explicit WizardController(ConfigManager *config, WakeManager *wake,
                              AlarmAudioController *audio,
                              QObject *parent = nullptr);

    int currentStep() const;
    int stepCount() const;
    bool canGoBack() const;
    // Next is never gated: the user may leave the wake page without testing.
    bool canGoNext() const;
    bool wizardCompleted() const;
    QString language() const;
    QString wakeStatus() const;
    bool notificationsAvailable() const;
    QVariantList outputDevices() const;
    bool mirrorApp() const;
    void setMirrorApp(bool v);
    bool mirrorWindowButtons() const;
    void setMirrorWindowButtons(bool v);
    int emojiLevel() const;
    void setEmojiLevel(int level);

    // How many steps the wizard has (kStepCount). QML reads it, never hardcodes.
    static constexpr int kStepCount = 5;

    // WakeManager::armTestWake() accepts only [60, 3600] s, so the shortest
    // non-destructive probe is 60 s — a 5 s request would be refused outright.
    // Mirrors WakeManager::kTestWakeMinSeconds, which is private, and the
    // constant lives here rather than in WakeManager because STG-5.5 must not
    // change that class.
    static constexpr int kWakeProbeSeconds = 60;

    // Step indices, so the QML and the tests agree on what "step 1" means.
    // Language, wake method, notification probe, alarm sound, then the summary.
    enum Step {
        Language = 0,
        WakeMethod = 1,
        Notifications = 2,
        AlarmSound = 3,
        Done = 4
    };

    Q_INVOKABLE void next();
    Q_INVOKABLE void back();
    // Skip: mark completed (so it never auto-shows again) and hide. It writes
    // nothing else — the language persisted itself the moment it was picked
    // (retranslation is live).
    Q_INVOKABLE void skip();
    // Finish: mark completed and hide. Idempotent by the same guard.
    Q_INVOKABLE void finish();
    // Page 6 secondary link: close the wizard for this run WITHOUT writing
    // wizard/completed, so the next launch shows it again.
    Q_INVOKABLE void finishRepeatNextTime();
    // Settings > "Run setup wizard again": clear the flag, rewind to step 0
    // and make the overlay reappear.
    Q_INVOKABLE void reset();

    // Page 1: the language selector. Writes ConfigManager's single ui/language
    // key; main.cpp already swaps the catalog and retranslates on change, so
    // the new language takes effect immediately.
    void setLanguage(const QString &lang);

    // Page 2: arm the shortest wake WakeManager accepts, report the outcome,
    // then cancel it immediately — non-destructive by construction. Next does
    // not depend on this.
    Q_INVOKABLE bool testWake();
    // Page 2: explicit "Skip this step".
    Q_INVOKABLE void skipWakeStep();
    // Page 3: send one benign notification; false when there is no daemon.
    Q_INVOKABLE bool testNotification();

    // Page 4: ask the audio controller for the current sink list, then play
    // one short tone so the chosen sound can be judged before finishing.
    // Both delegate to the existing controller; the wizard adds no audio code.
    Q_INVOKABLE void refreshOutputDevices();
    Q_INVOKABLE void previewSound(const QString &toneName);
    // First-run only: play the locally generated chime when no tone files are
    // installed yet, so the sound page's Preview button always works.
    Q_INVOKABLE void previewDefaultTone();
    // Setup preview through the real alarm volume policy. stopRing() restores
    // the sink when the same button is pressed again.
    Q_INVOKABLE void previewAlarmVolume(const QString &toneName);
    Q_INVOKABLE void previewDefaultAlarmVolume();
    Q_INVOKABLE void stopAlarmPreview();

    // Pages 2 and 5: plain-language description of the wake mechanism
    // WakeManager reports for this machine.
    Q_INVOKABLE QString detectWakeMethod() const;

    // --show-wizard: force the wizard visible for this run regardless of the
    // persisted flag, without rewriting it.
    void setForceShow(bool force);

Q_SIGNALS:
    void currentStepChanged();
    void wizardCompletedChanged();
    void languageChanged();
    void wakeStatusChanged();
    void notificationProbeChanged();
    void outputDevicesChanged();
    void configChangedMirror();

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
    AlarmAudioController *m_audio;
    int m_currentStep = 0;
    bool m_completed = false;
    bool m_forceShow = false;
    bool m_wakeProbePassed = false;
    bool m_wakeProbeSkipped = false;
    QString m_wakeStatus;
    bool m_notificationsAvailable = false;
};

#endif // WIZARDCONTROLLER_H
