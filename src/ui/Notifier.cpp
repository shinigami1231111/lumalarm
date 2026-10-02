#include "ui/Notifier.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariantMap>

Notifier::Notifier(bool enabled, QObject *parent)
    : QObject(parent)
    , m_enabled(enabled)
{
}

bool Notifier::isEnabled() const
{
    return m_enabled;
}

void Notifier::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool Notifier::deliver(const QString &title, const QString &body, int timeoutMs)
{
    // Disabled: no bus work, no logging, warn flag untouched (D4/F3).
    if (!m_enabled)
        return false;

    // Built per call and never cached, so a daemon that appears later is
    // picked up without a restart (D3).
    QDBusInterface iface(QStringLiteral("org.freedesktop.Notifications"),
                         QStringLiteral("/org/freedesktop/Notifications"),
                         QStringLiteral("org.freedesktop.Notifications"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        const QDBusError err = iface.lastError();
        if (!m_warnedUnavailable) {
            qWarning() << "[Notifier] notification daemon unreachable:" << err.name()
                       << "-" << err.message();
            m_warnedUnavailable = true;
        }
        return false;
    }

    // Signature per the freedesktop notification specification:
    //   Notify(s app_name, u replaces_id, s app_icon, s summary, s body,
    //          as actions, a{sv} hints, i expire_timeout) -> (u id)
    // expire_timeout -1 = server default; anything <= 0 from a caller means
    // "use the default" (0 would otherwise mean "never expire").
    const QDBusMessage raw = iface.call(QStringLiteral("Notify"),
                                        QStringLiteral("Lumalarm"),
                                        uint(0),                    // replaces_id
                                        QStringLiteral("lumalarm"),  // app_icon
                                        title,
                                        body,
                                        QStringList(),              // actions
                                        QVariantMap(),              // hints
                                        timeoutMs <= 0 ? -1 : timeoutMs);
    if (raw.type() == QDBusMessage::ErrorMessage) {
        if (!m_warnedUnavailable) {
            qWarning() << "[Notifier] Notify rejected:" << raw.errorName()
                       << raw.errorMessage();
            m_warnedUnavailable = true;
        }
        return false;
    }

    const QDBusReply<uint> reply(raw);
    if (!reply.isValid()) {
        if (!m_warnedUnavailable) {
            qWarning() << "[Notifier] Notify returned an invalid reply";
            m_warnedUnavailable = true;
        }
        return false;
    }

    // A daemon answered: clear the suppression so a later failure is reported.
    m_warnedUnavailable = false;
    return true;
}

void Notifier::notify(const QString &title, const QString &body, int timeoutMs)
{
    deliver(title, body, timeoutMs);
}

void Notifier::notifyAlarmUpcoming(const QString &label, int minutesUntil)
{
    deliver(QStringLiteral("Alarm in %1 min").arg(minutesUntil), label, 8000);
}

void Notifier::notifyWakeFired()
{
    deliver(QStringLiteral("Alarm"), QStringLiteral("Time to wake up"), 10000);
}

void Notifier::notifyTestWakeArmed(int seconds)
{
    deliver(QStringLiteral("Test alarm armed"),
            QStringLiteral("This machine will try to wake in %1 s").arg(seconds), 6000);
}
