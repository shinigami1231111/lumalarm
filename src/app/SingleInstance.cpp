#include "app/SingleInstance.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QtLogging>

namespace {
constexpr char KService[] = "io.github.shinigami1231111.Lumalarm";
constexpr char KPath[] = "/io/github/shinigami1231111/Lumalarm";
} // namespace

SingleInstance::SingleInstance(QObject *parent) : QObject(parent)
{
}

bool SingleInstance::registerService()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        // No session bus (bare TTY, minimal container): skip D-Bus entirely and
        // let the QLockFile guard do the job as it always has.
        qInfo() << "[SingleInstance] no session bus — falling back to the lock file";
        return true;
    }

    if (!bus.registerService(QLatin1String(KService))) {
        qInfo() << "[SingleInstance] service already owned by another instance";
        return false;
    }

    // Export only the slots above (Activate), not properties or signals.
    bus.registerObject(QLatin1String(KPath), this, QDBusConnection::ExportAllSlots);
    return true;
}

bool SingleInstance::notifyExisting()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    QDBusInterface iface(QLatin1String(KService), QLatin1String(KPath),
                         QLatin1String("io.github.shinigami1231111.Lumalarm"), bus);
    if (!iface.isValid()) {
        qWarning() << "[SingleInstance] cannot reach the running instance:"
                   << iface.lastError().message();
        return false;
    }

    const QDBusMessage reply = iface.call(QStringLiteral("Activate"));
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qWarning() << "[SingleInstance] Activate() failed:" << reply.errorName();
        return false;
    }
    qInfo() << "[SingleInstance] asked the running instance to show its window";
    return true;
}

void SingleInstance::Activate()
{
    // Runs on the D-Bus call; emitting is all we do — main.cpp connects this to
    // lifecycle->showMainWindow() with Qt::QueuedConnection (I4).
    emit activated();
}
