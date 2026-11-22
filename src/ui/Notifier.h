#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QDBusConnection>
#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;

// Desktop notifications for the alarm flows, spoken directly over the
// freedesktop.org notification protocol (org.freedesktop.Notifications on the
// SESSION bus). No libnotify dependency and no new build requirement; Qt6::DBus
// is already linked by the lumalarm target.
//
// Failure policy: best effort, never blocks, never throws. Notify() is sent with
// QDBusConnection::asyncCall(), so an absent or hung notification daemon can
// never delay the alarm-trigger signal chain or the QML ringing overlay. The
// first failure is logged once at warning level (including the daemon's error
// text) and further warnings are suppressed until a successful delivery clears
// that flag. The bus is queried on every call, so a daemon that appears later is
// picked up without a restart.
//
// Ownership: an instance is owned by main.cpp as a plain stack local (no QObject
// parent), mirroring how blurManager and scheduler are already composed there.
// Nothing in this class reaches back into Scheduler, WakeManager or ConfigManager.

class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(bool enabled = true,
                      const QDBusConnection &bus = QDBusConnection::sessionBus(),
                      QObject *parent = nullptr);

    // Master switch, mirrored from ConfigManager::notificationsEnabled().
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // Free-form notification. timeoutMs <= 0 asks the daemon for its default.
    void notify(const QString &title, const QString &body, int timeoutMs = 5000);

    // Convenience wrappers for the Lumalarm flows.
    void notifyAlarmUpcoming(const QString &label, int minutesUntil);
    void notifyWakeFired();
    void notifyTestWakeArmed(int seconds);

private slots:
    void onNotifyFinished(QDBusPendingCallWatcher *watcher);

private:
    // Queues one Notify() call. Returns false only when disabled or when the
    // bus is disconnected; delivery errors are reported asynchronously.
    bool deliver(const QString &title, const QString &body, int timeoutMs);
    void warnUnavailable(const QString &detail);

    bool m_enabled;
    bool m_warnedUnavailable = false;
    QDBusConnection m_bus;
};

#endif // NOTIFIER_H
