#ifndef FAKEMIXER_H
#define FAKEMIXER_H

#include "audio/SystemMixer.h"

// Deterministic test double (tests only, never linked into the app): all
// requests complete synchronously by emitting the interface signals, and
// tests can inject failures and simulate external change events.
class FakeMixer : public SystemMixer
{
    Q_OBJECT
public:
    explicit FakeMixer(QObject *parent = nullptr);
    ~FakeMixer() override = default;

    // --- test setup -------------------------------------------------------
    void setSink(const SinkState &sink);
    void addSinkInput(const SinkInputInfo &input);
    void setNextVolumeResult(bool success);
    void setNextMuteResult(bool success);
    void setNextMoveResult(bool success);
    void setDevices(const QList<SinkState> &devices);
    void setCapabilities(const MixerCapabilities &capabilities);
    void simulateSinkChange(const SinkState &sink);
    void simulateSinkInputChange(const SinkInputInfo &input);
    void simulateDefaultSinkChange(const QString &name);

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

    // --- inspection for tests --------------------------------------------
    QVector<int> lastAppliedVolumes() const { return m_lastAppliedVolumes; }
    bool lastAppliedMute() const { return m_lastAppliedMute; }
    int lastSinkInputIndex() const { return m_lastSinkInputIndex; }
    QVector<int> lastSinkInputVolumes() const { return m_lastSinkInputVolumes; }
    int applyVolumeCallCount() const { return m_applyVolumeCallCount; }
    int applyMuteCallCount() const { return m_applyMuteCallCount; }
    SinkState sink() const { return m_sink; }
    int listSinksCallCount() const { return m_listSinksCallCount; }
    int moveCallCount() const { return m_moveCallCount; }
    int lastMovedIndex() const { return m_lastMovedIndex; }
    QString lastMovedSink() const { return m_lastMovedSink; }

private:
    SinkState m_sink;
    QVector<SinkInputInfo> m_sinkInputs;
    QList<SinkState> m_devices;
    MixerCapabilities m_capabilities;
    bool m_nextVolumeOk = true;
    bool m_nextMuteOk = true;
    bool m_nextMoveOk = true;

    QVector<int> m_lastAppliedVolumes;
    bool m_lastAppliedMute = false;
    int m_lastSinkInputIndex = -1;
    QVector<int> m_lastSinkInputVolumes;
    int m_applyVolumeCallCount = 0;
    int m_applyMuteCallCount = 0;
    int m_listSinksCallCount = 0;
    int m_moveCallCount = 0;
    int m_lastMovedIndex = -1;
    QString m_lastMovedSink;
};

#endif // FAKEMIXER_H
