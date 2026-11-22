#include "ui/Notifier.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>
#include <QtLogging>

namespace {
// The pending-call lifetime, not a wait imposed on the GUI thread. asyncCall()
// returns immediately; the watcher is freed when the daemon answers or when
// this timeout expires.
constexpr int kNotifyPendingTimeoutMs = 5000;
} // namespace

Notifier::Notifier(bool enabled, const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_enabled(enabled), m_bus(bus)
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

void Notifier::warnUnavailable(const QString &detail)
{
    if (m_warnedUnavailable)
        return;
    qWarning() << "[Notifier] notification daemon unreachable:" << detail;
    m_warnedUnavailable = true;
}

bool Notifier::deliver(const QString &title, const QString &body, int timeoutMs)
{
    // Disabled: no bus work, no logging, warn flag untouched (D4/F3).
    if (!m_enabled)
        return false;

    if (!m_bus.isConnected()) {
        warnUnavailable(QStringLiteral("no session bus connection"));
        return false;
    }

    // Signature per the freedesktop notification specification:
    //   Notify(s app_name, u replaces_id, s app_icon, s summary, s body,
    //          as actions, a{sv} hints, i expire_timeout) -> (u id)
    // expire_timeout -1 = server default; anything <= 0 from a caller means
    // "use the default" (0 would otherwise mean "never expire").
    //
    // This must never use QDBusInterface::call() here: a blocking D-Bus call
    // inside the alarmTriggered signal chain stalls the GUI thread before QML
    // can show the ringing overlay. asyncCall() queues the message and returns.
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    msg << QStringLiteral("Lumalarm") << quint32(0) << QStringLiteral("lumalarm") << title << body
        << QStringList() << QVariantMap() << (timeoutMs <= 0 ? -1 : timeoutMs);

    const QDBusPendingCall pending = m_bus.asyncCall(msg, kNotifyPendingTimeoutMs);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &Notifier::onNotifyFinished);
    return true;
}

void Notifier::onNotifyFinished(QDBusPendingCallWatcher *watcher)
{
    const QDBusPendingReply<quint32> reply(*watcher);
    if (reply.isError()) {
        warnUnavailable(reply.error().name() + QStringLiteral(" - ") + reply.error().message());
    } else {
        // A daemon answered: clear the suppression so a later failure is reported.
        m_warnedUnavailable = false;
    }
    watcher->deleteLater();
}

void Notifier::notify(const QString &title, const QString &body, int timeoutMs)
{
    deliver(title, body, timeoutMs);
}

void Notifier::notifyAlarmUpcoming(const QString &label, int minutesUntil)
{
    deliver(tr("Alarm in %1 min").arg(minutesUntil), label, 8000);
}

void Notifier::notifyWakeFired()
{
    deliver(tr("Alarm"), tr("Time to wake up"), 10000);
}

void Notifier::notifyTestWakeArmed(int /*seconds*/)
{
    // A test wake only brings the machine back up — nothing rings. The body is
    // empty on purpose: the user reads this AFTER waking, when "will wake in
    // 60 s" would already be history.
    deliver(tr("Machine armed"), QString(), 6000);
}
