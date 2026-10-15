#include "audio/MprisPause.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariant>

#include <utility>

namespace {

constexpr int kCallTimeoutMs = 200;
const QLatin1String kPlayerServicePrefix("org.mpris.MediaPlayer2.");
const QLatin1String kPlayerObjectPath("/org/mpris/MediaPlayer2");
const QLatin1String kPlayerInterface("org.mpris.MediaPlayer2.Player");

} // namespace

MprisPause::MprisPause(QObject *parent)
    : QObject(parent)
{
}

QStringList MprisPause::playerServices(const QStringList &registeredNames)
{
    QStringList players;
    for (const QString &name : registeredNames) {
        if (name.startsWith(kPlayerServicePrefix))
            players.append(name);
    }
    players.sort();
    return players;
}

bool MprisPause::sessionAvailable() const
{
    return QDBusConnection::sessionBus().isConnected();
}

void MprisPause::pauseAll()
{
    m_paused.clear();

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return;

    const QDBusReply<QStringList> reply =
        bus.interface()->registeredServiceNames();
    if (!reply.isValid())
        return;

    for (const QString &service : playerServices(reply.value())) {
        QDBusInterface player(service, kPlayerObjectPath, kPlayerInterface);
        player.setTimeout(kCallTimeoutMs);
        // Remote property via the dynamic meta object; an unavailable
        // property yields an empty variant, which simply is not "Playback".
        const QString status =
            player.property("PlaybackStatus").toString();
        if (status != QLatin1String("Playback"))
            continue; // paused/stopped: not our business
        player.asyncCall(QStringLiteral("Pause")); // fire-and-forget
        m_paused.append(service);
    }
}

void MprisPause::resumeAll()
{
    if (m_paused.isEmpty())
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const QString &service : std::as_const(m_paused)) {
        QDBusInterface player(service, kPlayerObjectPath, kPlayerInterface);
        player.setTimeout(kCallTimeoutMs);
        player.asyncCall(QStringLiteral("Play")); // fire-and-forget
    }
    m_paused.clear();
}
