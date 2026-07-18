#ifndef WAKEMANAGER_H
#define WAKEMANAGER_H

#include <QObject>
#include <QProcess>

class WakeManager : public QObject
{
    Q_OBJECT

public:
    explicit WakeManager(QObject *parent = nullptr);

    Q_INVOKABLE void prepareWake(int secondsUntilAlarm, const QString &mode = "mem");
    Q_INVOKABLE bool isRtcWakeAvailable() const;

signals:
    void wakePrepared();
    void wakeError(const QString &message);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_process;
};

#endif // WAKEMANAGER_H
