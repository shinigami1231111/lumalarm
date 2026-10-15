#ifndef NULLMIXER_H
#define NULLMIXER_H

#include "audio/SystemMixer.h"

class NullMixer : public SystemMixer
{
    Q_OBJECT
public:
    explicit NullMixer(QObject *parent = nullptr);
    ~NullMixer() override = default;

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
};

#endif // NULLMIXER_H