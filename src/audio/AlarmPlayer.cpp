#include "audio/AlarmPlayer.h"
#include "audio/VolumePolicy.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>

AlarmPlayer::AlarmPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_verifyTimer(new QTimer(this))
{
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(static_cast<float>(m_streamVolume) / 100.0f);

    m_verifyTimer->setSingleShot(true);
    connect(m_verifyTimer, &QTimer::timeout, this, &AlarmPlayer::attemptVerify);
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                const bool wasPlaying = m_playing;
                m_playing = false;
                if (wasPlaying)
                    emit isPlayingChanged();
                emit playbackError(message);
            });
}

void AlarmPlayer::setMixer(SystemMixer *mixer)
{
    if (m_mixer == mixer)
        return;
    if (m_mixer)
        disconnect(m_mixer, nullptr, this, nullptr);
    m_mixer = mixer;
    if (!m_mixer)
        return;
    connect(m_mixer, &SystemMixer::sinkInputsReady, this,
            &AlarmPlayer::onSinkInputs);
    connect(m_mixer, &SystemMixer::sinkInputsError, this,
            [this](const QString &reason) {
                if (!m_verifying)
                    return; // someone else's request, or verification ended
                // The backend cannot list streams (e.g. amixer): stop
                // politely - the alarm still rings, we just cannot verify.
                qInfo() << "alarm stream not controllable:" << reason;
                finishVerify(false);
            });
    // A failed move never cancels the ring: the stream simply stays on the
    // default device (still audible), verification continues as before.
    connect(m_mixer, &SystemMixer::moveError, this, [this](const QString &reason) {
        qWarning() << "alarm stream not routed to" << m_outputSink << ":"
                   << reason << "- ringing on the default device";
    });
}

void AlarmPlayer::setStreamVolume(int percent)
{
    const int clamped = VolumePolicy::clampStreamPercent(percent);
    if (clamped == m_streamVolume)
        return;
    m_streamVolume = clamped;
    m_audioOutput->setVolume(static_cast<float>(clamped) / 100.0f);
}

int AlarmPlayer::streamVolume() const
{
    return m_streamVolume;
}

float AlarmPlayer::outputVolume() const
{
    return m_audioOutput->volume();
}

void AlarmPlayer::setOutputSink(const QString &sinkName)
{
    m_outputSink = sinkName.trimmed();
}

QString AlarmPlayer::outputSink() const
{
    return m_outputSink;
}

bool AlarmPlayer::isPlaying() const
{
    return m_playing;
}

void AlarmPlayer::play(const QString &filePath)
{
    stop();

    const QFileInfo info(filePath);
    if (filePath.isEmpty() || !info.isFile()) {
        emit playbackError(QStringLiteral("Sound file not found: ")
                           + filePath);
        return;
    }

    m_playing = true;
    m_moved = false; // at most one routing attempt per ring
    startMedia(filePath, true);
    emit isPlayingChanged();
    beginVerification();
}

void AlarmPlayer::preview(const QString &filePath)
{
    stop();

    const QFileInfo info(filePath);
    if (filePath.isEmpty() || !info.isFile()) {
        emit playbackError(QStringLiteral("Sound file not found: ")
                           + filePath);
        return;
    }

    m_playing = true;
    startMedia(filePath, false);
    emit isPlayingChanged();
}

void AlarmPlayer::stop()
{
    const bool wasPlaying = m_playing;
    m_playing = false;
    // Cancel verification silently: the user stopped the alarm, a
    // "could not verify" result would be noise.
    m_verifying = false;
    m_verifyTimer->stop();
    stopMedia();
    if (wasPlaying)
        emit isPlayingChanged();
}

void AlarmPlayer::setVerifyAttempts(int attempts)
{
    m_verifyAttempts = qMax(1, attempts);
}

void AlarmPlayer::setVerifyIntervalMs(int intervalMs)
{
    m_verifyIntervalMs = qMax(1, intervalMs);
}

void AlarmPlayer::startMedia(const QString &filePath, bool looping)
{
    m_player->stop();
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->setLoops(looping ? QMediaPlayer::Infinite
                               : QMediaPlayer::Once);
    m_player->play();
}

void AlarmPlayer::stopMedia()
{
    m_player->stop();
}

void AlarmPlayer::reinitializeAudio()
{
    auto *oldOut = m_audioOutput;
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    const float volume = oldOut ? oldOut->volume()
                                : static_cast<float>(m_streamVolume) / 100.0f;
    m_audioOutput->setVolume(volume);
    if (oldOut)
        oldOut->deleteLater();
}

void AlarmPlayer::beginVerification()
{
    if (!m_mixer)
        return; // no mixer: playback only, no verification signal
    m_verifyTries = 0;
    m_verifyEmitted = false;
    m_verifying = true;
    attemptVerify();
}

void AlarmPlayer::attemptVerify()
{
    if (!m_verifying || !m_playing)
        return;
    if (m_verifyTries >= m_verifyAttempts) {
        qWarning() << "alarm stream volume could not be verified within"
                   << m_verifyAttempts << "tries";
        finishVerify(false);
        return;
    }
    ++m_verifyTries;
    m_mixer->listSinkInputs(); // answered via onSinkInputs()
}

void AlarmPlayer::onSinkInputs(const QVector<SinkInputInfo> &inputs)
{
    if (!m_verifying)
        return; // an answer to someone else's request, or verification ended

    const SinkInputInfo *own = findOwnStream(inputs);
    if (!own) {
        // The stream may not exist yet (PipeWire creates it when playback
        // actually starts) - keep trying until the window runs out.
        m_verifyTimer->start(m_verifyIntervalMs);
        return;
    }

    // Speaker selector: move OUR stream to the chosen device once it is
    // located. Per-stream volume survives the move (and the next verify
    // attempt re-checks it anyway), so ordering here is not load-bearing.
    if (!m_moved && !m_outputSink.isEmpty()) {
        m_moved = true;
        m_mixer->moveSinkInput(own->index, m_outputSink);
    }

    const QVector<int> target =
        targetRawForChannels(own->volumes.size(), m_streamVolume);
    if (!own->mute && VolumePolicy::volumesMatch(own->volumes, target)) {
        finishVerify(true);
        return;
    }

    // stream-restore hazard in action: force our stream to the requested
    // level (never above 100 %), then re-check on the next try.
    m_mixer->setSinkInputVolume(own->index, target);
    if (own->mute)
        m_mixer->setSinkInputMute(own->index, false);
    m_verifyTimer->start(m_verifyIntervalMs);
}

void AlarmPlayer::finishVerify(bool ok)
{
    m_verifying = false;
    m_verifyTimer->stop();
    if (m_verifyEmitted)
        return;
    m_verifyEmitted = true;
    emit streamVerified(ok);
}

const SinkInputInfo *
AlarmPlayer::findOwnStream(const QVector<SinkInputInfo> &inputs)
{
    // Process id is the robust key where a backend provides it (pactl);
    // wpctl does not, so the application name is the fallback.
    const qint64 myPid = QCoreApplication::applicationPid();
    const QString myName = QCoreApplication::applicationName();
    const SinkInputInfo *byName = nullptr;
    for (const SinkInputInfo &input : inputs) {
        if (input.processId == myPid)
            return &input;
        if (!byName && !myName.isEmpty()
            && input.applicationName == myName)
            byName = &input;
    }
    return byName;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
QVector<int> AlarmPlayer::targetRawForChannels(qsizetype channels, int percent)
{
    const int raw = VolumePolicy::clampStreamPercent(percent)
                    * VolumePolicy::kFullScale / 100;
    return QVector<int>(qMax(qsizetype(1), channels), raw);
}
