#ifndef SYSTEMMIXER_H
#define SYSTEMMIXER_H

#include <QObject>
#include <QString>
#include <QVector>

// Data model for one audio sink (system volume/mute/port), shared by every
// backend. Raw volumes are PulseAudio-style: 65536 == 100 %, per channel,
// in the sink's channel-map order (never above 100 %, no amplification).
struct SinkPort {
    QString name;
    QString description;
    // "speaker" | "headphones" | "headset" | "bluetooth" | "hdmi" | "other" | "unknown"
    QString type;
    bool available = false;
    int priority = 0;
};

struct SinkState {
    int index = -1;          // PipeWire node id (stable across pactl/wpctl)
    QString name;            // node.name, e.g. alsa_output.pci-0000_00_1b.0.analog-stereo
    QString description;
    QVector<int> volumes;    // raw, per channel, channel-map order
    bool mute = false;
    SinkPort activePort;
    QVector<SinkPort> ports;
    QString state;           // "RUNNING" | "IDLE" | "SUSPENDED" | "UNKNOWN"
};

// One playback stream (a sink-input): our own alarm sound or another app's.
// Indexes are dynamic, so streams are matched by name/process, never by
// trusting a stored index across calls.
struct SinkInputInfo {
    int index = -1;
    int sinkIndex = -1;
    QString applicationName;
    QString nodeName;
    qint64 processId = -1;
    QVector<int> volumes;    // raw, per channel
    bool mute = false;
    bool corked = false;
};

// What a backend can actually do. The policy layer must stay within these
// (e.g. wpctl cannot do per-channel volumes, so nothing per-channel may be
// requested from it).
struct MixerCapabilities {
    bool canSetVolume = false;
    bool canMute = false;
    bool perChannel = false;
    bool hasSinkInputs = false;
    bool canSubscribe = false;
};

Q_DECLARE_METATYPE(MixerCapabilities)
Q_DECLARE_METATYPE(SinkState)
Q_DECLARE_METATYPE(QList<SinkState>)
Q_DECLARE_METATYPE(SinkInputInfo)
Q_DECLARE_METATYPE(QVector<SinkInputInfo>)

// Abstract mixer interface. All calls are asynchronous: results arrive via
// signals, never as return values. Every request gets exactly one response
// signal (ready/applied or error), so callers can always unblock.
//
// Guide backend order: pactl -> wpctl -> amixer -> NullMixer.
class SystemMixer : public QObject
{
    Q_OBJECT
public:
    explicit SystemMixer(QObject *parent = nullptr);
    ~SystemMixer() override = default;

    // Presence-based pick (fast, synchronous): the first CLI tool found on
    // PATH wins. The chosen backend still has to prove itself via detect()
    // (a pactl binary without a running pipewire-pulse socket fails there),
    // and the controller falls through to the next candidate.
    static SystemMixer *create(QObject *parent = nullptr);
    static SystemMixer *createBackend(const QString &backendName, QObject *parent = nullptr);

    // Probe the backend. Always answers with backendDetected(name, caps);
    // empty caps mean "present but unusable".
    virtual void detect() = 0;

    // Default sink (what the controller manages). Emits sinkReady or sinkError.
    virtual void getDefaultSink() = 0;
    // Specific sink by node.name. Emits sinkReady or sinkError.
    virtual void getSink(const QString &name) = 0;

    // Set per-channel raw volumes (channel-map order). Emits volumeApplied
    // (echoes the sink name) or volumeError.
    virtual void setSinkVolumes(const QString &sinkName, const QVector<int> &volumes) = 0;
    virtual void setSinkMute(const QString &sinkName, bool mute) = 0; // muteApplied/muteError

    // Streams. Emits sinkInputsReady (list, possibly empty) or sinkInputsError.
    virtual void listSinkInputs() = 0;
    virtual void setSinkInputVolume(int index, const QVector<int> &volumes) = 0;
    virtual void setSinkInputMute(int index, bool mute) = 0;

    // Device discovery + routing (speaker selector). listSinks emits
    // sinksReady with name/description/index filled (volumes may be empty
    // where enumerating them cheaply is not possible — the UI only needs
    // identity). moveSinkInput routes OUR stream (sink-input index) to the
    // named sink; it never changes the system default. Emits moveApplied
    // (echoes the index) or moveError.
    virtual void listSinks() = 0;
    virtual void moveSinkInput(int index, const QString &sinkName) = 0;

    // Change notifications: sinkChanged (volume/mute/port of one sink),
    // defaultSinkChanged (default switched to another sink), sinkInputChanged
    // (a stream's volume/mute changed). Backends without an event source may
    // poll while watching; watching is off until startWatch().
    virtual void startWatch() = 0;
    virtual void stopWatch() = 0;

signals:
    void backendDetected(const QString &backendName, const MixerCapabilities &capabilities);
    void sinkReady(const SinkState &sink);
    void sinkError(const QString &reason);
    void volumeApplied(const QString &sinkName);
    void volumeError(const QString &reason);
    void muteApplied(const QString &sinkName);
    void muteError(const QString &reason);
    void sinkInputsReady(const QVector<SinkInputInfo> &inputs);
    void sinkInputsError(const QString &reason);
    void sinkInputVolumeApplied(int index);
    void sinkInputVolumeError(const QString &reason);
    void sinkInputMuteApplied(int index);
    void sinkInputMuteError(const QString &reason);
    void sinkChanged(const SinkState &sink);
    void defaultSinkChanged(const QString &name);
    void sinkInputChanged(const SinkInputInfo &input);
    void sinksReady(const QList<SinkState> &sinks);
    void sinksError(const QString &reason);
    void moveApplied(int index);
    void moveError(const QString &reason);
};

#endif // SYSTEMMIXER_H
