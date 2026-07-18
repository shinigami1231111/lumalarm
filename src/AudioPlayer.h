#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>

class AudioPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int baseVolume READ baseVolume WRITE setBaseVolume NOTIFY baseVolumeChanged)
    Q_PROPERTY(int fadeDuration READ fadeDuration WRITE setFadeDuration NOTIFY fadeDurationChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() = default;

    int volume() const;
    void setVolume(int percent);

    int baseVolume() const;
    void setBaseVolume(int percent);

    int fadeDuration() const;
    void setFadeDuration(int seconds);

    bool isPlaying() const;

    Q_INVOKABLE void play(const QString &filePath);
    Q_INVOKABLE void preview(const QString &filePath);
    Q_INVOKABLE void stop();

signals:
    void volumeChanged();
    void baseVolumeChanged();
    void fadeDurationChanged();
    void isPlayingChanged();
    void playbackError(const QString &message);

private slots:
    void onFadeTick();
    void onMediaError(QMediaPlayer::Error error);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QTimer *m_fadeTimer;

    int m_volume = 80;
    int m_baseVolume = 20;
    int m_fadeDuration = 15;
    int m_currentFadeVolume = 0;

    void startFade();
};

#endif // AUDIOPLAYER_H
