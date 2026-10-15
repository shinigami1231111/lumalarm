#include "audio/NullMixer.h"

#include <QDebug>

// Fallback used when no CLI tool is available at all: every request degrades
// to an error signal so callers notice, but nothing ever crashes and nothing
// ever touches the system.
NullMixer::NullMixer(QObject *parent)
    : SystemMixer(parent)
{
}

void NullMixer::detect()
{
    qInfo() << "Lumalarm: no mixer CLI (pactl/wpctl/amixer) available —"
            << "system volume control disabled";
    emit backendDetected(QStringLiteral("null"), MixerCapabilities{});
}

void NullMixer::getDefaultSink()
{
    emit sinkError(QStringLiteral("System volume control unavailable (no pactl/wpctl/amixer)"));
}

void NullMixer::getSink(const QString &name)
{
    Q_UNUSED(name);
    emit sinkError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::setSinkVolumes(const QString &sinkName, const QVector<int> &volumes)
{
    Q_UNUSED(sinkName);
    Q_UNUSED(volumes);
    emit volumeError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::setSinkMute(const QString &sinkName, bool mute)
{
    Q_UNUSED(sinkName);
    Q_UNUSED(mute);
    emit muteError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::listSinkInputs()
{
    emit sinkInputsError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::setSinkInputVolume(int index, const QVector<int> &volumes)
{
    Q_UNUSED(index);
    Q_UNUSED(volumes);
    emit sinkInputVolumeError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::setSinkInputMute(int index, bool mute)
{
    Q_UNUSED(index);
    Q_UNUSED(mute);
    emit sinkInputMuteError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::listSinks()
{
    emit sinksError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::moveSinkInput(int index, const QString &sinkName)
{
    Q_UNUSED(index);
    Q_UNUSED(sinkName);
    emit moveError(QStringLiteral("System volume control unavailable"));
}

void NullMixer::startWatch()
{
}

void NullMixer::stopWatch()
{
}
