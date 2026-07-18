#include "AudioPlayer.h"
#include <QUrl>
#include <QFileInfo>
#include <QStandardPaths>

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

void AudioPlayer::play(const QString &filePath)
{
    m_fadeTimer->stop();
    m_player->stop();

    if (filePath.isEmpty()) return;

    QString resolved = filePath;
    if (!resolved.contains('/') && !resolved.contains('\\')) {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm/tones";
        resolved = configDir + "/" + resolved;
    }

    QFileInfo fi(resolved);
    if (!fi.isFile()) return;

    m_player->setSource(QUrl::fromLocalFile(resolved));
    m_player->play();
    startFade();
    emit isPlayingChanged();
}

void AudioPlayer::preview(const QString &filePath)
{
    m_player->stop();
    m_fadeTimer->stop();

    if (filePath.isEmpty()) return;

    QString resolved = filePath;
    if (!resolved.contains('/') && !resolved.contains('\\')) {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm/tones";
        resolved = configDir + "/" + resolved;
    }

    QFileInfo fi(resolved);
    if (!fi.isFile()) return;

    m_player->setSource(QUrl::fromLocalFile(resolved));
    m_audioOutput->setVolume(m_volume / 100.0);
    m_player->play();
    emit isPlayingChanged();
}

void AudioPlayer::stop()
{
    m_fadeTimer->stop();
    m_player->stop();
    m_audioOutput->setVolume(m_volume / 100.0);
    emit isPlayingChanged();
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

void AudioPlayer::onMediaError(QMediaPlayer::Error error)
{
    Q_UNUSED(error);
    QString msg = m_player->errorString();
    emit playbackError(msg);
}

void AudioPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_fadeTimer->isActive()) {
            m_fadeTimer->stop();
            m_audioOutput->setVolume(m_volume / 100.0);
        }
        emit isPlayingChanged();
    }
}

void AudioPlayer::startFade()
{
    m_currentFadeVolume = m_baseVolume;
    double startVolume = (m_volume / 100.0) * (m_baseVolume / 100.0);
    m_audioOutput->setVolume(startVolume);

    m_fadeTimer->start(100);
}
