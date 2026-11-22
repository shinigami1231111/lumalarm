#ifndef ALARMAUDIOCONTROLLER_H
#define ALARMAUDIOCONTROLLER_H

#include "audio/AlarmPlayer.h"
#include "audio/RestoreJournal.h"
#include "audio/SoundResolver.h"
#include "audio/SystemMixer.h"
#include "audio/VolumePolicy.h"

#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

class MprisPause;
class QTemporaryDir;

// The single owner of everything the alarm sound does at runtime. Replaces
// AudioPlayer (deleted with Issue #1) and wires the rebuilt layers together:
//   SystemMixer      - real sink/stream state (pactl -> wpctl -> amixer)
//   VolumePolicy     - what to raise, tolerance, clamps
//   RestoreJournal   - crash-safe write-ahead restore
//   AlarmPlayer      - the stream itself (verified/forced)
//   MprisPause       - other players stop talking over the alarm
//   SoundResolver    - per-alarm -> global default -> first tone -> silent
//
// Flow of a ring under the guarantee policy:
//   1. resolve the sound; an empty result rings the window silently;
//   2. read the default sink; write the journal BEFORE any change;
//   3. raise/unmute to the floor, watch for user overrides (3 % tolerance);
//   4. play the stream (its sink-input is verified/forced separately);
//   5. on stop: restore the sink exactly, clear the journal, resume players.
// The quit path runs step 5 bounded (<= ~800 ms); a timeout leaves the
// journal in place and next start's recover() finishes the job.
//
// The QML surface deliberately keeps the old AudioPlayer API — play /
// preview / stop / isPlaying / playbackError — so the context property
// `audioPlayer` in QML keeps its name and its call sites.
class AlarmAudioController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    // Speaker selector list: [{name: "node.name", description: "..."}, ...],
    // refreshed async by refreshOutputDevices().
    Q_PROPERTY(QVariantList outputDevices READ outputDevices NOTIFY
                   outputDevicesChanged)

public:
    struct Settings {
        VolumePolicy::Kind policy = VolumePolicy::Kind::Guarantee;
        int floorPercent = 50;   // guarantee: sink floor, 0..100
        int streamPercent = 100; // our stream volume, 0..100 (never > 1.0)
        QString defaultSound;    // global default tone name ("" = none)
        QString outputSink;      // alarm output device ("" = system default)
    };

    // Production: builds every collaborator. Nothing I/O-bound happens until
    // start() (backend probe + crash recovery) or the first ring().
    explicit AlarmAudioController(QObject *parent = nullptr);
    // Test seam: everything injected; the player must be a media-seam
    // subclass and the MPRIS hook a no-op. Nothing here creates a real
    // mixer, a real journal location, or a real media backend.
    AlarmAudioController(SystemMixer *mixer, AlarmPlayer *player,
                         RestoreJournal *journal, const SoundResolver &resolver,
                         MprisPause *mpris, QObject *parent = nullptr);
    ~AlarmAudioController() override;

    // Probe the backend and finish crash recovery (journal.recover()).
    // Called once from main() after construction.
    void start();

    void applySettings(const Settings &settings);

    // --- QML surface (old AudioPlayer API; resolves tone names) -----------
    Q_INVOKABLE void play(const QString &toneName);    // looped, no sink policy
    Q_INVOKABLE void preview(const QString &toneName); // one shot
    // First-run fallback used only by the wizard's alarm-sound preview: when
    // no tone files are installed yet, generate a short WAV in a temporary
    // directory and play it through the same preview path. No external command
    // and no file in the user's tones folder.
    Q_INVOKABLE void previewDefaultTone();
    // Setup preview variants: same volume policy as a real alarm ring, so the
    // chosen "Raise system volume" / minimum-volume settings can be judged.
    // stopRing() ends them and restores the sink exactly.
    Q_INVOKABLE void previewWithAlarmVolume(const QString &toneName);
    Q_INVOKABLE void previewDefaultToneWithAlarmVolume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopRing();
    Q_INVOKABLE void reinitializeAudio(); // system resume
    bool isPlaying() const;

    // Speaker selector: ask the backend for the device list. Asynchronous —
    // outputDevicesChanged() follows (sinksError is logged, list unchanged).
    Q_INVOKABLE void refreshOutputDevices();
    QVariantList outputDevices() const;

    // --- the ring (C++ driven by main.cpp) --------------------------------
    void ring(const QString &perAlarmSound);

    // Bounded synchronous teardown for AppLifecycle::quit(): stop the
    // stream and restore the sink within ~800 ms; on timeout the journal
    // survives and recover() at next start completes the restore.
    void shutdownForQuit();

signals:
    void playbackError(const QString &message);
    void isPlayingChanged();
    // Restore finished (true) or gave up and kept the journal (false).
    void restoreSettled(bool completed);
    void outputDevicesChanged();

private:
    void connectMixer(SystemMixer *mixer);
    QString ensureFallbackTonePath();
    void onBackendDetected(const QString &backendName,
                           const MixerCapabilities &capabilities);
    void onSinksReady(const QList<SinkState> &sinks);
    void tryNextBackend();
    void onRingSinkReady(const SinkState &sink);
    void onSinkChanged(const SinkState &sink);
    void startRestore();
    void onRestoreVolumeApplied();
    void onRestoreMuteApplied();
    void onRestoreError(const QString &what);

    SystemMixer *m_mixer = nullptr;
    AlarmPlayer *m_player = nullptr;
    RestoreJournal *m_journal = nullptr;
    MprisPause *m_mpris = nullptr;
    SoundResolver m_resolver;
    Settings m_settings;

    bool m_ownsMixer = false;
    bool m_ownsPlayer = false;
    bool m_ownsJournal = false;

    QVariantList m_outputDevices; // last listSinks() answer
    QScopedPointer<QTemporaryDir> m_fallbackToneDir; // wizard preview WAV

    QStringList m_backendCandidates; // guide order, minus what failed
    QString m_currentBackendName;
    bool m_backendResolved = false;

    bool m_ringing = false;          // an alarm ring is active
    bool m_awaitingRingSink = false; // sink read in flight for management
    bool m_managing = false;         // journal written; sink is ours to restore
    bool m_overridden = false;       // user took over; never restore
    bool m_restoring = false;        // restore in flight
    bool m_recoveryBusy = false;     // journal.recover() in flight

    SinkState m_managedSink;      // the sink we raised, with ORIGINAL state
    QVector<int> m_appliedVolumes; // what we wrote (override comparison)
    bool m_appliedMute = false;
};

#endif // ALARMAUDIOCONTROLLER_H
