#ifndef WPCTLMIXER_H
#define WPCTLMIXER_H

#include "audio/MixerBackend.h"
#include "audio/SystemMixer.h"

#include <QHash>
#include <QTimer>

// Fallback backend: wpctl (WirePlumber). No event subscription, so watching
// polls `wpctl status` and only reports actual changes. Uniform volume only
// (caps.perChannel = false) and no stream (sink-input) control at all.
class WpctlMixer : public SystemMixer
{
    Q_OBJECT
public:
    struct WpSink {
        int id = -1;
        QString description;
        float volume = 1.0f;
        bool mute = false;
        bool isDefault = false;
    };

    explicit WpctlMixer(QObject *parent = nullptr);
    ~WpctlMixer() override;

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

    // --- pure parsers, exercised by tst_systemmixer against live captures
    static QVector<WpSink> parseStatus(const QString &output);
    static float parseVolumeLine(const QString &output, bool *muted = nullptr);
    static QString parseNodeName(const QString &output);
    static int parseAudioChannels(const QString &output);

private:
    // The default-sink fetch is a 3-step chain: status -> get-volume ->
    // inspect. Purpose and request-local values travel through captures.
    enum class Purpose { DefaultRequest, NameLookup, Poll };

    void fetchDefault(Purpose purpose, int id = -1, const QString &name = QString());
    void handleStatus(Purpose purpose, const QString &wantedName, const QByteArray &out,
                      const QByteArray &err, int exitCode, bool timedOut);
    void handleGetVolume(Purpose purpose, int id, const QString &name, const QString &description,
                         float statusVolume, bool statusMute, const QByteArray &out,
                         const QByteArray &err, int exitCode, bool timedOut);
    void handleInspect(Purpose purpose, int id, const QString &fallbackName,
                       const QString &description, float volume, bool mute,
                       const QByteArray &out, const QByteArray &err, int exitCode,
                       bool timedOut);

    void runWpctl(const QStringList &args, MixerBackend::Callback callback);
    int resolveId(const QString &nodeName) const;

    MixerBackend *m_backend = nullptr;
    QTimer *m_pollTimer = nullptr;

    // Caches filled by inspect() (wpctl addresses nodes by ID only).
    QHash<int, QString> m_idToNodeName;
    QHash<QString, int> m_nameToId;
    QHash<int, QString> m_idToDescription;

    // Last emitted default-sink state (for change detection).
    QString m_defaultSinkName;
    int m_defaultSinkId = -1;
    float m_defaultVolume = -1.0f;
    bool m_defaultMute = false;
};

#endif // WPCTLMIXER_H
