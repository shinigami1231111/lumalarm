#ifndef PACTLMIXER_H
#define PACTLMIXER_H

#include "audio/MixerBackend.h"
#include "audio/SystemMixer.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QStringList>

// Primary backend: pactl over the PipeWire PulseAudio-compatible socket.
// JSON output where available (`--format=json`), defensive text fallback for
// `pactl info`. Long-lived `pactl subscribe` provides change events.
class PactlMixer : public SystemMixer
{
    Q_OBJECT
public:
    explicit PactlMixer(QObject *parent = nullptr);
    ~PactlMixer() override;

    // --- SystemMixer interface -------------------------------------------
    void detect() override;
    void getDefaultSink() override;
    void getSink(const QString &name) override;
    void setSinkVolumes(const QString &sinkName, const QVector<int> &volumes) override;
    void setSinkMute(const QString &sinkName, bool mute) override;
    void listSinkInputs() override;
    void setSinkInputVolume(int index, const QVector<int> &volumes) override;
    void setSinkInputMute(int index, bool mute) override;
    void listSinks() override;
    void moveSinkInput(int index, const QString &sinkName) override;
    void startWatch() override;
    void stopWatch() override;

    // --- pure parsers, exercised by tst_systemmixer against Phase 0 captures
    static SinkState parseSink(const QJsonObject &object);
    static SinkInputInfo parseSinkInput(const QJsonObject &object);
    // Handles both `pactl --format=json info` and the plain text form.
    static QString parseInfoDefaultSink(const QByteArray &infoOutput);
    static QString portType(const QString &type, const QJsonObject &properties);
    // Volumes in channel-map order (QJsonObject keys are alphabetically
    // sorted, which would shuffle channels on the way back out).
    static QVector<int> parseVolumes(const QJsonObject &volumeObject,
                                     const QString &channelMap);
    static int parseVolumeValue(const QJsonValue &value);

private:
    enum class ListPurpose { DefaultSink, LookupByName, ChangedSink, AllDevices };

    // Command handlers; one per enqueued CLI call. Request-local state
    // (purpose, wanted name/index) travels through the lambda capture, never
    // through members, so queued commands cannot overwrite each other.
    void handleDetect(const QByteArray &out, const QByteArray &err, int exitCode, bool timedOut);
    void handleInfo(const QByteArray &out, const QByteArray &err, int exitCode, bool timedOut);
    void handleInfoText(const QByteArray &out, const QByteArray &err, int exitCode, bool timedOut);
    void handleSinkList(ListPurpose purpose, const QString &wantedName, int wantedIndex,
                        const QByteArray &out, const QByteArray &err, int exitCode,
                        bool timedOut);
    void handleSinkInputs(int wantedInputIndex, const QByteArray &out, const QByteArray &err,
                          int exitCode, bool timedOut);

    void runPactl(const QStringList &args, MixerBackend::Callback callback);
    void fetchSinks(ListPurpose purpose, const QString &wantedName, int wantedIndex);
    void onSubscribeLine(const QByteArray &line);

    QStringList pactlArgsSuffix() const; // "--server <unix:...>" or empty

    QString m_pulseServer;
    MixerBackend *m_backend = nullptr;
    QProcess *m_subscribeProc = nullptr;
    bool m_watchWanted = false;

    // Bookkeeping for change notifications (single writer: the FIFO queue).
    QString m_defaultSinkName;
};

#endif // PACTLMIXER_H
