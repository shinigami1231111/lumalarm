#include "audio/FakeMixer.h"

FakeMixer::FakeMixer(QObject *parent)
    : SystemMixer(parent)
{
    m_sink.index = 1;
    m_sink.name = QStringLiteral("fake-sink");
    m_sink.description = QStringLiteral("Fake Sink");
    m_sink.volumes = {65536, 65536};
    m_sink.mute = false;
    m_sink.state = QStringLiteral("RUNNING");

    m_capabilities.canSetVolume = true;
    m_capabilities.canMute = true;
    m_capabilities.perChannel = true;
    m_capabilities.hasSinkInputs = true;
    m_capabilities.canSubscribe = true;
}

void FakeMixer::setSink(const SinkState &sink)
{
    m_sink = sink;
}

void FakeMixer::addSinkInput(const SinkInputInfo &input)
{
    m_sinkInputs.append(input);
}

void FakeMixer::setNextVolumeResult(bool success)
{
    m_nextVolumeOk = success;
}

void FakeMixer::setNextMuteResult(bool success)
{
    m_nextMuteOk = success;
}

void FakeMixer::setNextMoveResult(bool success)
{
    m_nextMoveOk = success;
}

void FakeMixer::setDevices(const QList<SinkState> &devices)
{
    m_devices = devices;
}

void FakeMixer::setCapabilities(const MixerCapabilities &capabilities)
{
    m_capabilities = capabilities;
}

void FakeMixer::simulateSinkChange(const SinkState &sink)
{
    m_sink = sink;
    emit sinkChanged(sink);
}

void FakeMixer::simulateSinkInputChange(const SinkInputInfo &input)
{
    for (int i = 0; i < m_sinkInputs.size(); ++i) {
        if (m_sinkInputs.at(i).index == input.index) {
            m_sinkInputs[i] = input;
            emit sinkInputChanged(input);
            return;
        }
    }
    m_sinkInputs.append(input);
    emit sinkInputChanged(input);
}

void FakeMixer::simulateDefaultSinkChange(const QString &name)
{
    m_sink.name = name;
    emit defaultSinkChanged(name);
}

// --- SystemMixer interface -------------------------------------------------

void FakeMixer::detect()
{
    emit backendDetected(QStringLiteral("fake"), m_capabilities);
}

void FakeMixer::getDefaultSink()
{
    emit sinkReady(m_sink);
}

void FakeMixer::getSink(const QString &name)
{
    if (m_sink.name == name)
        emit sinkReady(m_sink);
    else
        emit sinkError(QStringLiteral("sink not found: ") + name);
}

void FakeMixer::setSinkVolumes(const QString &sinkName, const QVector<int> &volumes)
{
    ++m_applyVolumeCallCount;
    m_lastAppliedVolumes = volumes;

    if (!m_nextVolumeOk) {
        emit volumeError(QStringLiteral("fake volume failure"));
        return;
    }
    m_sink.volumes = volumes;
    emit volumeApplied(sinkName);
}

void FakeMixer::setSinkMute(const QString &sinkName, bool mute)
{
    ++m_applyMuteCallCount;
    m_lastAppliedMute = mute;

    if (!m_nextMuteOk) {
        emit muteError(QStringLiteral("fake mute failure"));
        return;
    }
    m_sink.mute = mute;
    emit muteApplied(sinkName);
}

void FakeMixer::listSinkInputs()
{
    emit sinkInputsReady(m_sinkInputs);
}

void FakeMixer::setSinkInputVolume(int index, const QVector<int> &volumes)
{
    m_lastSinkInputIndex = index;
    m_lastSinkInputVolumes = volumes;

    for (SinkInputInfo &input : m_sinkInputs) {
        if (input.index == index) {
            input.volumes = volumes;
            emit sinkInputVolumeApplied(index);
            return;
        }
    }
    emit sinkInputVolumeError(QStringLiteral("sink-input not found"));
}

void FakeMixer::setSinkInputMute(int index, bool mute)
{
    m_lastSinkInputIndex = index;

    for (SinkInputInfo &input : m_sinkInputs) {
        if (input.index == index) {
            input.mute = mute;
            emit sinkInputMuteApplied(index);
            return;
        }
    }
    emit sinkInputMuteError(QStringLiteral("sink-input not found"));
}

void FakeMixer::startWatch()
{
}

void FakeMixer::stopWatch()
{
}

void FakeMixer::listSinks()
{
    ++m_listSinksCallCount;
    emit sinksReady(m_devices);
}

void FakeMixer::moveSinkInput(int index, const QString &sinkName)
{
    ++m_moveCallCount;
    m_lastMovedIndex = index;
    m_lastMovedSink = sinkName;

    if (!m_nextMoveOk) {
        emit moveError(QStringLiteral("fake move failure"));
        return;
    }
    emit moveApplied(index);
}
