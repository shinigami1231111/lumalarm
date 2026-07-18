#include "WakeManager.h"
#include <QDebug>

WakeManager::WakeManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WakeManager::onProcessFinished);
}

void WakeManager::prepareWake(int secondsUntilAlarm, const QString &mode)
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    QStringList args;
    args << "-m" << mode << "-s" << QString::number(secondsUntilAlarm);
    m_process->start("sudo", QStringList() << "rtcwake" << args);
}

bool WakeManager::isRtcWakeAvailable() const
{
    QProcess test;
    test.start("which", QStringList() << "rtcwake");
    test.waitForFinished(2000);
    return test.exitCode() == 0;
}

void WakeManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::NormalExit && exitCode == 0) {
        emit wakePrepared();
    } else {
        QString err = m_process->readAllStandardError();
        if (err.isEmpty()) {
            err = "rtcwake failed with exit code " + QString::number(exitCode);
        }
        emit wakeError(err);
    }
}
