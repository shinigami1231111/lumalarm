#include "audio/AmixerMixer.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>

AmixerMixer::AmixerMixer(QObject *parent)
    : SystemMixer(parent)
    , m_backend(new MixerBackend(this))
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() { fetchState(Purpose::Poll); });
}

AmixerMixer::~AmixerMixer()
{
    stopWatch();
}

void AmixerMixer::runAmixer(const QStringList &args, MixerBackend::Callback callback)
{
    m_backend->enqueue(QStringLiteral("amixer"), args, std::move(callback));
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

void AmixerMixer::detect()
{
    runAmixer({QStringLiteral("scontrols")},
              // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
              [this](const QByteArray &out, const QByteArray &err, int code, bool timedOut) {
                  if (mixerCommandFailed(code, timedOut)) {
                      qWarning() << "Lumalarm: amixer detect failed:"
                                 << mixerFailureReason(err, code, timedOut);
                      emit backendDetected(QStringLiteral("amixer"), MixerCapabilities{});
                      return;
                  }
                  if (!QString::fromUtf8(out).contains(QLatin1String(kControl))) {
                      // Runs, but has no Master control we could manage.
                      emit backendDetected(QStringLiteral("amixer"), MixerCapabilities{});
                      return;
                  }
                  MixerCapabilities caps;
                  caps.canSetVolume = true;
                  caps.canMute = true;
                  caps.perChannel = false;
                  caps.hasSinkInputs = false;
                  caps.canSubscribe = false;
                  emit backendDetected(QStringLiteral("amixer"), caps);
              });
}

void AmixerMixer::getDefaultSink()
{
    fetchState(Purpose::Request);
}

void AmixerMixer::getSink(const QString &name)
{
    if (name != QLatin1String(kControl)) {
        emit sinkError(QStringLiteral("amixer backend only knows the Master control"));
        return;
    }
    fetchState(Purpose::Request);
}

void AmixerMixer::fetchState(Purpose purpose)
{
    runAmixer({QStringLiteral("sget"), QString::fromLatin1(kControl)},
              [this, purpose](const QByteArray &out, const QByteArray &err, int code,
                              bool timedOut) {
                  handleState(purpose, out, err, code, timedOut);
              });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void AmixerMixer::handleState(Purpose purpose, const QByteArray &out, const QByteArray &err,
                              int exitCode, bool timedOut)
{
    if (mixerCommandFailed(exitCode, timedOut)) {
        const QString reason = mixerFailureReason(err, exitCode, timedOut);
        if (purpose == Purpose::Request)
            emit sinkError(reason);
        else
            qWarning() << "Lumalarm: amixer refresh failed:" << reason;
        return;
    }

    const auto [percent, muted] = parseAmixerOutput(QString::fromUtf8(out));

    if (purpose == Purpose::Poll && percent == m_lastPercent && muted == m_lastMute)
        return; // unchanged

    SinkState state;
    state.index = 0; // ALSA has no node id; the control name is the address
    state.name = QString::fromLatin1(kControl);
    state.description = QStringLiteral("ALSA Master");
    const int raw = qBound(0, qRound(percent * 65536.0 / 100.0), 65536);
    state.volumes = QVector<int>(2, raw); // channel count unknown: assume stereo
    state.mute = muted;
    state.state = QStringLiteral("UNKNOWN");

    m_lastPercent = percent;
    m_lastMute = muted;

    if (purpose == Purpose::Request) {
        if (state.name != m_defaultSinkName) {
            m_defaultSinkName = state.name;
            emit defaultSinkChanged(state.name);
        }
        emit sinkReady(state);
    } else {
        emit sinkChanged(state);
    }
}

void AmixerMixer::setSinkVolumes(const QString &sinkName, const QVector<int> &volumes)
{
    if (volumes.isEmpty()) {
        emit volumeError(QStringLiteral("no volume values"));
        return;
    }
    if (sinkName != QLatin1String(kControl) && !sinkName.isEmpty()) {
        emit volumeError(QStringLiteral("amixer backend only knows the Master control"));
        return;
    }

    // Percent granularity, clamped to 100 % (no software amplification).
    const int percent = qBound(0, qRound(volumes.first() * 100.0 / 65536.0), 100);
    runAmixer({QStringLiteral("sset"), QString::fromLatin1(kControl),
               QString::number(percent) + QLatin1Char('%')},
              [this, sinkName](const QByteArray &, const QByteArray &err, int code,
                               bool timedOut) {
                  if (mixerCommandFailed(code, timedOut))
                      emit volumeError(mixerFailureReason(err, code, timedOut));
                  else
                      emit volumeApplied(sinkName);
              });
}

void AmixerMixer::setSinkMute(const QString &sinkName, bool mute)
{
    if (sinkName != QLatin1String(kControl) && !sinkName.isEmpty()) {
        emit muteError(QStringLiteral("amixer backend only knows the Master control"));
        return;
    }
    runAmixer({QStringLiteral("sset"), QString::fromLatin1(kControl),
               mute ? QStringLiteral("mute") : QStringLiteral("unmute")},
              [this, sinkName](const QByteArray &, const QByteArray &err, int code,
                               bool timedOut) {
                  if (mixerCommandFailed(code, timedOut))
                      emit muteError(mixerFailureReason(err, code, timedOut));
                  else
                      emit muteApplied(sinkName);
              });
}

void AmixerMixer::listSinkInputs()
{
    emit sinkInputsError(QStringLiteral("amixer backend cannot list audio streams"));
}

void AmixerMixer::setSinkInputVolume(int, const QVector<int> &)
{
    emit sinkInputVolumeError(QStringLiteral("amixer backend cannot control audio streams"));
}

void AmixerMixer::setSinkInputMute(int, bool)
{
    emit sinkInputMuteError(QStringLiteral("amixer backend cannot control audio streams"));
}

void AmixerMixer::listSinks()
{
    emit sinksError(QStringLiteral("amixer backend cannot enumerate audio devices"));
}

void AmixerMixer::moveSinkInput(int, const QString &)
{
    emit moveError(QStringLiteral("amixer backend cannot route audio streams"));
}

void AmixerMixer::startWatch()
{
    m_pollTimer->start();
    fetchState(Purpose::Poll);
}

void AmixerMixer::stopWatch()
{
    m_pollTimer->stop();
    m_backend->cancelAll();
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

QPair<int, bool> AmixerMixer::parseAmixerOutput(const QString &output)
{
    // Stereo:
    //   Front Left: Playback 32768 [50%] [-18.00dB] [on]
    //   Front Right: Playback 32768 [50%] [-18.00dB] [off]
    // Mono:
    //   Mono: Playback 65536 [100%] [0.00dB] [on]
    static const QRegularExpression channelLine(
        QStringLiteral(R"(Playback\s+\d+\s+\[(\d+)%\].*\[(on|off)\])"));
    static const QRegularExpression anyLine(
        QStringLiteral(R"(\[(\d+)%\].*\[(on|off)\])"));

    int percentSum = 0;
    int channelCount = 0;
    bool anyMuted = false;
    bool anyOn = false;

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        QRegularExpressionMatch match = channelLine.match(line);
        if (!match.hasMatch())
            match = anyLine.match(line);
        if (!match.hasMatch())
            continue;
        percentSum += match.captured(1).toInt();
        ++channelCount;
        if (match.captured(2) == QLatin1String("off"))
            anyMuted = true;
        else
            anyOn = true;
    }

    if (channelCount == 0)
        return {100, false}; // nothing parsed: report full volume, unmuted

    // Muted only if every channel is muted (matches how we set it).
    const bool muted = anyMuted && !anyOn;
    return {percentSum / channelCount, muted};
}
