#include "ui/WizardController.h"

#include "audio/AlarmAudioController.h"
#include "managers/ConfigManager.h"
#include "wake/WakeManager.h"

#include <QDebug>
#include <QFile>
#include <QVariantMap>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

namespace {
// The probe length itself lives on WizardController (kWakeProbeSeconds): it
// mirrors WakeManager's private bound, so STG-5.5 can honour it without
// touching WakeManager at all.

const char *kNotificationsService = "org.freedesktop.Notifications";
const char *kNotificationsPath = "/org/freedesktop/Notifications";
const char *kNotificationsInterface = "org.freedesktop.Notifications";
} // namespace

WizardController::WizardController(ConfigManager *config, WakeManager *wake, AlarmAudioController *audio,
                                   QObject *parent)
    : QObject(parent), m_config(config), m_wake(wake), m_audio(audio)
{
    // Dependencies are passed explicitly; never rediscovered through parent().
    if (!m_config || !m_wake || !m_audio)
        qWarning() << "[Wizard] constructed with a null dependency —"
                      "the wizard will not function";

    // Start on a completed run as completed, unless --show-wizard forces it on.
    m_completed = m_forceShow ? false : (m_config && m_config->wizardCompleted());

    // Mirror ConfigManager's configChanged so QML re-reads mirror/emoji flags
    // whenever they change.
    QObject::connect(m_config, &ConfigManager::configChanged, this,
                     &WizardController::configChangedMirror);

    probeNotifications();
}

void WizardController::setForceShow(bool force)
{
    // Called from main() before the QML engine loads main.qml. Force-show must
    // never rewrite the persisted flag; it only affects what is reported.
    m_forceShow = force;
    setCompleted(m_forceShow ? false : (m_config && m_config->wizardCompleted()));
}

int WizardController::currentStep() const
{
    return m_currentStep;
}

int WizardController::stepCount() const
{
    return kStepCount;
}

bool WizardController::canGoBack() const
{
    return m_currentStep > 0;
}

bool WizardController::canGoNext() const
{
    // Next is never gated: the user may leave any page without testing.
    return true;
}

bool WizardController::wizardCompleted() const
{
    return m_completed;
}

QString WizardController::language() const
{
    // Always the live ConfigManager value: the wizard holds no copy of its own.
    return m_config ? m_config->language() : QString();
}

void WizardController::setLanguage(const QString &lang)
{
    if (!m_config || m_config->language() == lang)
        return;
    // One key, one writer: ConfigManager::setLanguage() emits languageChanged,
    // which main.cpp answers by swapping the QTranslator and calling
    // engine.retranslate() — so RTL/LTR flips while this page is on screen.
    m_config->setLanguage(lang);
    emit languageChanged();
}

bool WizardController::mirrorApp() const
{
    return m_config ? m_config->mirrorApp() : false;
}

bool WizardController::mirrorWindowButtons() const
{
    return m_config ? m_config->mirrorWindowButtons() : false;
}

int WizardController::emojiLevel() const
{
    return m_config ? m_config->emojiLevel() : 2;
}

void WizardController::setMirrorApp(bool v)
{
    if (m_config) m_config->setMirrorApp(v);
}

void WizardController::setMirrorWindowButtons(bool v)
{
    if (m_config) m_config->setMirrorWindowButtons(v);
}

void WizardController::setEmojiLevel(int level)
{
    // ConfigManager's configChanged is already mirrored to configChangedMirror
    // in the constructor, so writing through is enough to notify QML once.
    if (m_config) m_config->setEmojiLevel(level);
}

QString WizardController::wakeStatus() const
{
    return m_wakeStatus;
}

bool WizardController::notificationsAvailable() const
{
    return m_notificationsAvailable;
}

void WizardController::next()
{
    if (!canGoNext())
        return;
    if (m_currentStep >= kStepCount - 1)
        return; // step 5 is the last one; Next is not offered
    setCurrentStep(m_currentStep + 1);

    // Re-probe the session bus when the notification page becomes current so
    // the answer reflects the machine as it is now, not as it was at startup.
    if (m_currentStep == Notifications)
        probeNotifications();
}

void WizardController::back()
{
    if (m_currentStep <= 0)
        return;
    setCurrentStep(m_currentStep - 1);
}

void WizardController::skip()
{
    if (!m_config)
        return;
    // Skipping records completion (so it never auto-shows again) and hides the
    // overlay, and writes nothing else. The language already persisted itself
    // the moment it was chosen; the sound and the wake probe are never
    // persisted, so there is nothing left to roll back.
    m_config->setWizardCompleted(true);
    setCompleted(true);
}

void WizardController::finishRepeatNextTime()
{
    // Closes the overlay for this run but deliberately leaves wizard/completed
    // untouched, so the next launch shows the wizard again.
    setCompleted(true);
}

void WizardController::finish()
{
    if (!m_config)
        return;
    // An explicit finish always ends the force-show override for this run.
    m_forceShow = false;
    m_config->setWizardCompleted(true);
    setCompleted(true);
}

void WizardController::reset()
{
    if (!m_config)
        return;
    m_forceShow = false;
    m_config->setWizardCompleted(false);
    setCompleted(false);
    m_wakeProbePassed = false;
    m_wakeProbeSkipped = false;
    m_wakeStatus.clear();
    emit wakeStatusChanged();
    emit currentStepChanged(); // canGoNext() gate changed with the probes
    setCurrentStep(Language);
    probeNotifications();
}

bool WizardController::testWake()
{
    if (!m_wake)
        return false;

    const bool unlockedBefore = m_wakeProbePassed || m_wakeProbeSkipped;
    const bool armed = armProbeWake();
    if (armed) {
        // Non-destructive: drop the wake the moment we know it worked.
        m_wake->cancelTestWake();
        m_wakeProbePassed = true;
    }

    const QString status = armed ? tr("Wake test succeeded — this machine can wake up.")
                                 : tr("Wake test failed: %1").arg(m_wake->lastError());
    if (m_wakeStatus != status) {
        m_wakeStatus = status;
        emit wakeStatusChanged();
    }
    // Compare the wake-page gate itself, not canGoNext() — the caller may be on
    // any page when the probe runs.
    if ((m_wakeProbePassed || m_wakeProbeSkipped) != unlockedBefore)
        emit currentStepChanged(); // Next button gate just opened
    return armed;
}

void WizardController::skipWakeStep()
{
    if (m_wakeProbeSkipped)
        return;
    m_wakeProbeSkipped = true;
    emit currentStepChanged();
}

bool WizardController::testNotification()
{
    // Per-call interface (never cached), exactly like src/ui/Notifier: a dead
    // or absent daemon must not leave a stale connection behind.
    QDBusInterface iface(kNotificationsService, kNotificationsPath, kNotificationsInterface,
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (m_notificationsAvailable) {
            m_notificationsAvailable = false;
            emit notificationProbeChanged();
        }
        return false;
    }

    // Harmless notification: no urgency, no actions, auto-expire.
    QDBusReply<quint32> reply = iface.call(QStringLiteral("Notify"),
                                           QStringLiteral("Lumalarm"),       // app_name
                                           quint32(0),                       // replaces_id
                                           QStringLiteral("alarm"),          // app_icon
                                           tr("Lumalarm is ready"),          // summary
                                           tr("Notifications are working."), // body
                                           QStringList(),                    // actions (as)
                                           QVariantMap(),                    // hints (a{sv})
                                           8000);                            // expire_timeout (ms)

    if (!reply.isValid())
        return false;
    if (!m_notificationsAvailable) {
        m_notificationsAvailable = true;
        emit notificationProbeChanged();
    }
    return true;
}

QString WizardController::detectWakeMethod() const
{
    if (!m_wake)
        return tr("Unknown");

    // Only what WakeManager already exposes — no new detection code and no
    // change to WakeManager itself (STG-5.5 must not touch its behaviour).
    if (m_wake->isRtcWakeAvailable())
        return tr("Real-time clock wake (rtcwake) — recommended");

    return tr("No hardware wake source — alarms ring only while the machine is awake");
}

bool WizardController::armProbeWake()
{
    // Armed, observed and cancelled at once; kWakeProbeSeconds is the shortest
    // request WakeManager::armTestWake() will accept (its range is [60, 3600]).
    return m_wake && m_wake->armTestWake(kWakeProbeSeconds);
}

void WizardController::setCurrentStep(int step)
{
    if (m_currentStep == step)
        return;
    m_currentStep = step;
    emit currentStepChanged();
}

void WizardController::setCompleted(bool completed)
{
    if (m_completed == completed)
        return;
    m_completed = completed;
    emit wizardCompletedChanged();
}

void WizardController::probeNotifications()
{
    // Pure presence check — no message is sent here.
    const QDBusConnection bus = QDBusConnection::sessionBus();
    bool available = false;
    if (bus.isConnected() && bus.interface())
        available =
            bus.interface()->isServiceRegistered(QString::fromLatin1(kNotificationsService));
    if (m_notificationsAvailable == available)
        return;
    m_notificationsAvailable = available;
    emit notificationProbeChanged();
}
