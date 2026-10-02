#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>
#include <QString>

// Desktop notifications for the alarm flows, spoken directly over the
// freedesktop.org notification protocol (org.freedesktop.Notifications on the
// SESSION bus) using QDBusInterface. No libnotify dependency and no new build
// requirement; Qt6::DBus is already linked by the lumalarm target.
//
// Failure policy: best effort, never blocks, never throws. Every call is
// attempted regardless of earlier failures; the first failure is logged once at
// warning level (including the daemon's error text) and further warnings are
// suppressed until a successful delivery clears that flag. The QDBusInterface
// is built per call and never cached, so a daemon that appears later is picked
// up without a restart.
//
// Ownership: an instance is owned by main.cpp as a plain stack local (no QObject
// parent), mirroring how blurManager and scheduler are already composed there.
// Nothing in this class reaches back into Scheduler, WakeManager or ConfigManager.

class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(bool enabled = true, QObject *parent = nullptr);

    // Master switch, mirrored from ConfigManager::notificationsEnabled().
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // Free-form notification. timeoutMs <= 0 asks the daemon for its default.
    void notify(const QString &title, const QString &body, int timeoutMs = 5000);

    // Convenience wrappers for the Lumalarm flows.
    void notifyAlarmUpcoming(const QString &label, int minutesUntil);
    void notifyWakeFired();
    void notifyTestWakeArmed(int seconds);

private:
    // Attempts one Notify() call. Returns true only when the daemon accepted
    // it; false when disabled, unreachable or rejected (already logged).
    bool deliver(const QString &title, const QString &body, int timeoutMs);

    bool m_enabled;
    bool m_warnedUnavailable = false;
};

#endif // NOTIFIER_H
