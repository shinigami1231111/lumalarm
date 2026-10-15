#ifndef ALARMPLAYER_H
#define ALARMPLAYER_H

#include "audio/SystemMixer.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>

// New alarm sound player (guide rebuild); replaces AudioPlayer in Phase 4.
//  * takes an already-resolved absolute path (SoundResolver does the lookup);
//  * play() rings in a loop until stop(), preview() plays once (no
//    verification - it is shorter than the verification window);
//  * stream volume 0-100, clamped through VolumePolicy: never above 1.0
//    (no software amplification);
//  * stream verification (guide: MANDATORY - stream-restore hazard): after
//    the ring starts, our own sink-input is located by process id (or
//    application name where a backend gives no pid) and its volume/mute are
//    forced to the requested stream volume, then re-checked. PipeWire
//    otherwise restores an old per-app stream volume (reproduced live in
//    Phase 0).
//  * output device (Phase 6, speaker selector): once our sink-input is
//    located, it is moved to the configured sink (node.name). Only OUR
//    stream moves - never the system default - and a failed move leaves the
//    ring audible on the default device. Preview follows the default sink.
//
// All media access goes through startMedia()/stopMedia() so tests never
// touch a real audio device.
class AlarmPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AlarmPlayer(QObject *parent = nullptr);
    ~AlarmPlayer() override = default;

    // Borrowed. Without a mixer (or without stream control on the backend)
    // playback still works; only verification is skipped. The owner runs
    // detect() on the mixer before ringing.
    void setMixer(SystemMixer *mixer);

    // 0-100 (clamped). Maps to QAudioOutput::setVolume(percent / 100.0), so
    // it can never exceed 1.0. Default 100.
    void setStreamVolume(int percent);
    int streamVolume() const;
    float outputVolume() const; // audible stream level, for assertions

    // Target device for the ring (node.name from SoundResolver/Settings);
    // "" = follow the system default. Applied once per play, right after
    // our sink-input is located during verification.
    void setOutputSink(const QString &sinkName);
    QString outputSink() const;

    bool isPlaying() const;

    void play(const QString &filePath);
    void preview(const QString &filePath);
    void stop();

    // Verification pacing (defaults: 8 tries x 250 ms). A sink-input appears
    // as soon as the stream starts, so the window only has to cover
    // scheduling jitter; tests shrink it.
    void setVerifyAttempts(int attempts);
    void setVerifyIntervalMs(int intervalMs);

signals:
    void playbackError(const QString &message);
    void isPlayingChanged();
    // Emitted at most once per play() once our sink-input matches the
    // requested stream volume (ok=true), or when the window ran out / the
    // backend has no stream control (ok=false). Never emitted without a
    // mixer.
    void streamVerified(bool ok);

protected:
    // Media seam: the real player uses QMediaPlayer; tests override.
    virtual void startMedia(const QString &filePath, bool looping);
    virtual void stopMedia();

private:
    void beginVerification();
    void attemptVerify();
    void onSinkInputs(const QVector<SinkInputInfo> &inputs);
    void finishVerify(bool ok);
    static const SinkInputInfo *
    findOwnStream(const QVector<SinkInputInfo> &inputs);
    static QVector<int> targetRawForChannels(qsizetype channels, int percent);

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QTimer *m_verifyTimer;
    SystemMixer *m_mixer = nullptr;

    int m_streamVolume = 100;
    int m_verifyAttempts = 8;
    int m_verifyIntervalMs = 250;
    int m_verifyTries = 0;
    bool m_playing = false;
    bool m_verifying = false;
    bool m_verifyEmitted = false;
    QString m_outputSink;
    bool m_moved = false; // one move attempt per play()

public slots:
    void reinitializeAudio(); // audio device change, as in AudioPlayer
};

#endif // ALARMPLAYER_H
