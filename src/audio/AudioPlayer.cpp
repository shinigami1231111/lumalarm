#include "audio/AudioPlayer.h"
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
{
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::errorOccurred, this, &AudioPlayer::onMediaError);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &AudioPlayer::onMediaStatusChanged);
    connect(m_fadeTimer, &QTimer::timeout, this, &AudioPlayer::onFadeTick);
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
            m_audioOutput->setVolume(static_cast<float>(m_volume / 100.0));
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

void AudioPlayer::reinitializeAudio()
{
    auto *oldOut = m_audioOutput;
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(
        static_cast<float>(oldOut ? oldOut->volume() : (m_volume / 100.0)));
    if (oldOut) {
        oldOut->deleteLater();
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
    m_audioOutput->setVolume(static_cast<float>(m_volume / 100.0));
    m_player->play();
    emit isPlayingChanged();
}

void AudioPlayer::stop()
{
    m_looping = false;
    m_fadeTimer->stop();
    m_player->stop();
    m_audioOutput->setVolume(static_cast<float>(m_volume / 100.0));
    emit isPlayingChanged();
}

void AudioPlayer::onFadeTick()
{
    m_currentFadeVolume =
        static_cast<int>(m_currentFadeVolume + (100 - m_baseVolume) / (m_fadeDuration * 10.0));
    if (m_currentFadeVolume >= 100) {
        m_currentFadeVolume = 100;
        m_fadeTimer->stop();
    }

    double effectiveVolume = (m_volume / 100.0) * (m_currentFadeVolume / 100.0);
    m_audioOutput->setVolume(static_cast<float>(effectiveVolume));
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
                m_audioOutput->setVolume(static_cast<float>(m_volume / 100.0));
            }
            emit isPlayingChanged();
        }
    }
}

void AudioPlayer::startFade()
{
    m_currentFadeVolume = m_baseVolume;
    double startVolume = (m_volume / 100.0) * (m_baseVolume / 100.0);
    m_audioOutput->setVolume(static_cast<float>(startVolume));

    m_fadeTimer->start(100);
}
