#include "audio/SystemMixer.h"

#include "audio/AmixerMixer.h"
#include "audio/NullMixer.h"
#include "audio/PactlMixer.h"
#include "audio/WpctlMixer.h"

#include <QStandardPaths>

SystemMixer::SystemMixer(QObject *parent)
    : QObject(parent)
{
}

SystemMixer *SystemMixer::create(QObject *parent)
{
    if (!QStandardPaths::findExecutable(QStringLiteral("pactl")).isEmpty())
        return createBackend(QStringLiteral("pactl"), parent);
    if (!QStandardPaths::findExecutable(QStringLiteral("wpctl")).isEmpty())
        return createBackend(QStringLiteral("wpctl"), parent);
    if (!QStandardPaths::findExecutable(QStringLiteral("amixer")).isEmpty())
        return createBackend(QStringLiteral("amixer"), parent);
    return new NullMixer(parent);
}

SystemMixer *SystemMixer::createBackend(const QString &backendName, QObject *parent)
{
    if (backendName == QLatin1String("pactl"))
        return new PactlMixer(parent);
    if (backendName == QLatin1String("wpctl"))
        return new WpctlMixer(parent);
    if (backendName == QLatin1String("amixer"))
        return new AmixerMixer(parent);
    return new NullMixer(parent);
}
