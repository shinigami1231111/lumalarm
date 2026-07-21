#include "AudioPlayer.h"
#include <QUrl>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMediaDevices>
#include <QAudioDevice>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_fadeTimer(new QTimer(this))
    , m_scPlayer(new QMediaPlayer(this))
    , m_scAudioOutput(new QAudioOutput(this))
    , m_scFadeTimer(new QTimer(this))
{
    m_player->setAudioOutput(m_audioOutput);
    m_scPlayer->setAudioOutput(m_scAudioOutput);

    // Explicitly bind to the system default output device so audio is never
    // routed to a null/muted device.
    auto dev = QMediaDevices::defaultAudioOutput();
    m_audioOutput->setDevice(dev);
    m_scAudioOutput->setDevice(dev);
    m_audioOutput->setVolume(m_volume / 100.0);
    m_scAudioOutput->setVolume(0);

    connect(m_player, &QMediaPlayer::errorOccurred, this, &AudioPlayer::onMediaError);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &AudioPlayer::onMediaStatusChanged);
    connect(m_fadeTimer, &QTimer::timeout, this, &AudioPlayer::onFadeTick);
    connect(m_scFadeTimer, &QTimer::timeout, this, &AudioPlayer::onSoundscapeFadeTick);
}

QString AudioPlayer::resolvePath(const QString &filePath) const
{
    if (filePath.isEmpty()) return {};
    QString resolved = filePath;
    if (!resolved.contains('/') && !resolved.contains('\\')) {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm/tones";
        resolved = configDir + "/" + resolved;
    }
    QFileInfo fi(resolved);
    return fi.isFile() ? resolved : QString();
}

int AudioPlayer::volume() const
{
    return m_volume;
}

void AudioPlayer::setVolume(int percent)
{
    if (m_volume != percent) {
        m_volume = qBound(0, percent, 100);
        if (!m_fadeTimer->isActive()) {
            m_audioOutput->setVolume(m_volume / 100.0);
        }
        emit volumeChanged();
    }
}

int AudioPlayer::baseVolume() const
{
    return m_baseVolume;
}

void AudioPlayer::setBaseVolume(int percent)
{
    if (m_baseVolume != percent) {
        m_baseVolume = qBound(0, percent, 50);
        emit baseVolumeChanged();
    }
}

int AudioPlayer::fadeDuration() const
{
    return m_fadeDuration;
}

void AudioPlayer::setFadeDuration(int seconds)
{
    if (m_fadeDuration != seconds) {
        m_fadeDuration = qBound(5, seconds, 60);
        emit fadeDurationChanged();
    }
}

bool AudioPlayer::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

bool AudioPlayer::isSoundscapePlaying() const
{
    return m_scPlayer->playbackState() == QMediaPlayer::PlayingState;
}

void AudioPlayer::reinitializeAudio()
{
    auto *oldOut = m_audioOutput;
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(oldOut ? oldOut->volume() : (m_volume / 100.0));
    if (oldOut) {
        oldOut->deleteLater();
    }

    auto *oldSc = m_scAudioOutput;
    m_scAudioOutput = new QAudioOutput(this);
    m_scPlayer->setAudioOutput(m_scAudioOutput);
    m_scAudioOutput->setVolume(oldSc ? oldSc->volume() : 0);
    if (oldSc) {
        oldSc->deleteLater();
    }
}

void AudioPlayer::play(const QString &filePath)
{
    m_looping = true;
    m_fadeTimer->stop();
    m_player->stop();

    QString resolved = resolvePath(filePath);
    if (resolved.isEmpty()) {
        emit playbackError("Sound file not found: " + filePath);
        return;
    }

    m_player->setSource(QUrl::fromLocalFile(resolved));
    m_player->play();
    startFade();
    emit isPlayingChanged();
}

void AudioPlayer::preview(const QString &filePath)
{
    m_player->stop();
    m_fadeTimer->stop();

    QString resolved = resolvePath(filePath);
    if (resolved.isEmpty()) {
        emit playbackError("Sound file not found: " + filePath);
        return;
    }

    m_player->setSource(QUrl::fromLocalFile(resolved));
    m_audioOutput->setVolume(m_volume / 100.0);
    m_player->play();
    emit isPlayingChanged();
}

void AudioPlayer::stop()
{
    m_looping = false;
    m_fadeTimer->stop();
    m_scFadeTimer->stop();
    m_player->stop();
    m_scPlayer->stop();
    m_audioOutput->setVolume(m_volume / 100.0);
    m_scAudioOutput->setVolume(0);
    m_crossfading = false;
    emit isPlayingChanged();
}

void AudioPlayer::playSoundscape(const QString &filePath, int startVolumePercent)
{
    m_scPlayer->stop();
    m_scFadeTimer->stop();

    QString resolved = resolvePath(filePath);
    if (resolved.isEmpty()) return;

    m_scTargetVolume = qBound(1, startVolumePercent, 30);
    m_scCurrentVolume = 0;

    m_scPlayer->setSource(QUrl::fromLocalFile(resolved));
    m_scPlayer->setLoops(QMediaPlayer::Infinite);
    m_scAudioOutput->setVolume(0);
    m_scPlayer->play();

    // Ramp soundscape volume over ~85 seconds from 0 to target
    m_scFadeTimer->start(100);
}

void AudioPlayer::stopSoundscape()
{
    m_scFadeTimer->stop();
    m_scPlayer->stop();
    m_scAudioOutput->setVolume(0);
    m_crossfading = false;
}

void AudioPlayer::crossfadeToMain(int mainBaseVolume, int mainFadeDuration)
{
    m_crossfading = true;
    setBaseVolume(mainBaseVolume);
    setFadeDuration(mainFadeDuration);

    // Start main alarm at current soundscape volume level
    double scVol = m_scAudioOutput->volume();
    m_currentFadeVolume = qRound(scVol * 100);
    m_audioOutput->setVolume(scVol);

    m_player->play();
    startFade();
}

void AudioPlayer::onFadeTick()
{
    m_currentFadeVolume += (100 - m_baseVolume) / (m_fadeDuration * 10.0);
    if (m_currentFadeVolume >= 100) {
        m_currentFadeVolume = 100;
        m_fadeTimer->stop();
    }

    double effectiveVolume = (m_volume / 100.0) * (m_currentFadeVolume / 100.0);
    m_audioOutput->setVolume(effectiveVolume);
}

void AudioPlayer::onSoundscapeFadeTick()
{
    if (m_crossfading) {
        // During crossfade, fade soundscape out
        double vol = m_scAudioOutput->volume();
        vol -= 0.005;
        if (vol <= 0) {
            vol = 0;
            m_scFadeTimer->stop();
            m_scPlayer->stop();
        }
        m_scAudioOutput->setVolume(vol);
        return;
    }

    m_scCurrentVolume += m_scTargetVolume / 850.0;
    if (m_scCurrentVolume >= m_scTargetVolume) {
        m_scCurrentVolume = m_scTargetVolume;
        m_scFadeTimer->stop();
    }
    m_scAudioOutput->setVolume(m_scCurrentVolume / 100.0);
}

void AudioPlayer::onMediaError(QMediaPlayer::Error error)
{
    Q_UNUSED(error);
    QString msg = m_player->errorString();
    emit playbackError(msg);
}

void AudioPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_looping) {
            m_player->setPosition(0);
            QTimer::singleShot(0, m_player, &QMediaPlayer::play);
        } else {
            if (m_fadeTimer->isActive()) {
                m_fadeTimer->stop();
                m_audioOutput->setVolume(m_volume / 100.0);
            }
            emit isPlayingChanged();
        }
    }
}

void AudioPlayer::startFade()
{
    m_currentFadeVolume = m_baseVolume;
    double startVolume = (m_volume / 100.0) * (m_baseVolume / 100.0);
    m_audioOutput->setVolume(startVolume);

    m_fadeTimer->start(100);
}
