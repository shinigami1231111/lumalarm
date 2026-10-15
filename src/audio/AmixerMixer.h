#ifndef AMIXERMIXER_H
#define AMIXERMIXER_H

#include "audio/MixerBackend.h"
#include "audio/SystemMixer.h"

#include <QPair>
#include <QTimer>

// Last-resort backend: ALSA's amixer, "Master" control only. Phase 0 showed
// it failing on PipeWire-only systems ("Host is down"), so everything here
// degrades to an error signal rather than assuming success. Uniform volume,
// no streams, no events (watching polls like wpctl).
class AmixerMixer : public SystemMixer
{
    Q_OBJECT
public:
    explicit AmixerMixer(QObject *parent = nullptr);
    ~AmixerMixer() override;

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

    // --- pure parser (tested) --------------------------------------------
    // Returns {percent 0..100, muted} from an `amixer sget Master` dump;
    // {100, false} when nothing parses (defensive: never crashes).
    static QPair<int, bool> parseAmixerOutput(const QString &output);

private:
    enum class Purpose { Request, Poll };

    void fetchState(Purpose purpose);
    void handleState(Purpose purpose, const QByteArray &out, const QByteArray &err,
                     int exitCode, bool timedOut);
    void runAmixer(const QStringList &args, MixerBackend::Callback callback);

    MixerBackend *m_backend = nullptr;
    QTimer *m_pollTimer = nullptr;

    static constexpr const char *kControl = "Master";

    // Last emitted state (for poll change detection).
    int m_lastPercent = -1;
    bool m_lastMute = false;
    QString m_defaultSinkName;
};

#endif // AMIXERMIXER_H
