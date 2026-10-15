#include "audio/MixerBackend.h"

#include <QProcessEnvironment>

MixerBackend::MixerBackend(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                // A crash or a kill both arrive here; the exit code plus our
                // timeout flag are all the caller needs.
                finishCurrent(exitCode, m_timedOut);
            });
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        // kill() is asynchronous: QProcess::finished fires right after the
        // process dies, which is where finishCurrent() runs with m_timedOut.
        m_timedOut = true;
        m_process->kill();
    });
}

void MixerBackend::enqueue(const QString &program, const QStringList &args, Callback callback,
                           int timeoutMs)
{
    m_queue.append(Item{program, args, timeoutMs, std::move(callback)});
    if (!m_running)
        startNext();
}

void MixerBackend::cancelAll()
{
    m_queue.clear();
    if (m_running) {
        m_cancelled = true;
        m_timeoutTimer->stop();
        m_process->kill();
        // finishCurrent() runs from QProcess::finished and skips the callback.
    }
}

void MixerBackend::startNext()
{
    if (m_queue.isEmpty()) {
        m_running = false;
        return;
    }

    m_current = m_queue.takeFirst();
    m_running = true;
    m_timedOut = false;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_process->setProcessEnvironment(env);
    m_process->start(m_current.program, m_current.args);

    if (!m_process->waitForStarted(200)) {
        // The binary is missing or not executable: report it as a failed
        // command (exitCode -1) so callers can degrade to the next backend.
        const QByteArray err =
            QByteArrayLiteral("failed to start ") + m_current.program.toUtf8();
        Callback cb = std::move(m_current.callback);
        m_current = Item{};
        m_running = false;
        if (cb && !m_cancelled)
            cb(QByteArray(), err, -1, false);
        if (!m_running) // the callback may have enqueued follow-up work
            startNext();
        return;
    }

    m_timeoutTimer->start(m_current.timeoutMs);
}

void MixerBackend::finishCurrent(int exitCode, bool timedOut)
{
    if (!m_running)
        return;

    m_timeoutTimer->stop();
    const QByteArray out = m_process->readAllStandardOutput();
    const QByteArray err = m_process->readAllStandardError();

    Callback cb = std::move(m_current.callback);
    m_current = Item{};
    m_running = false;
    const bool cancelled = m_cancelled;
    m_cancelled = false;
    m_timedOut = false;

    if (cb && !cancelled)
        cb(out, err, exitCode, timedOut);

    if (!m_running) // the callback may have enqueued follow-up work
        startNext();
}
