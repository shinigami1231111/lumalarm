#ifndef MIXERBACKEND_H
#define MIXERBACKEND_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>

// Asynchronous, queued runner for the mixer CLI tools (pactl / wpctl / amixer).
//
// Guide contract:
//  - argument lists only, never a shell string (no ;, $, ` interpretation)
//  - LC_ALL=C for every invocation (stable, untranslated output)
//  - 1.5 s default timeout; a hung tool is killed and reported as timedOut
//  - commands complete FIFO with exactly one callback each, so a slow tool
//    can never deliver its result under another command's handler
//  - never blocks the caller (except up to 200 ms while spawning)
class MixerBackend : public QObject
{
    Q_OBJECT
public:
    // exitCode is -1 when the program could not be started; stdErr then says
    // why. timedOut is true when the timeout fired and we killed the process.
    using Callback = std::function<void(const QByteArray &stdOut, const QByteArray &stdErr,
                                        int exitCode, bool timedOut)>;

    static constexpr int DefaultTimeoutMs = 1500;

    explicit MixerBackend(QObject *parent = nullptr);
    ~MixerBackend() override = default;

    // Queues a command (behind anything already running). The callback runs
    // from the event loop, exactly once per started command.
    void enqueue(const QString &program, const QStringList &args, Callback callback,
                 int timeoutMs = DefaultTimeoutMs);

    bool isBusy() const { return m_running; }

    // Drops every queued command and kills the running one; the killed
    // command's callback is NOT invoked. Used from stopWatch()/destructors.
    void cancelAll();

private:
    void startNext();
    void finishCurrent(int exitCode, bool timedOut);

    struct Item {
        QString program;
        QStringList args;
        int timeoutMs = DefaultTimeoutMs;
        Callback callback;
    };

    QProcess *m_process = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QList<Item> m_queue;
    Item m_current;
    bool m_running = false;
    bool m_timedOut = false;
    bool m_cancelled = false;
};

// Shared failure classification for the backends. Guide rule: timeout,
// non-zero exit and unparsable output must never crash, only degrade.
inline bool mixerCommandFailed(int exitCode, bool timedOut)
{
    return timedOut || exitCode != 0;
}

inline QString mixerFailureReason(const QByteArray &stdErr, int exitCode, bool timedOut)
{
    if (timedOut)
        return QStringLiteral("timed out");
    const QString err = QString::fromUtf8(stdErr).trimmed();
    if (!err.isEmpty())
        return err;
    return QStringLiteral("exit code %1").arg(exitCode);
}

#endif // MIXERBACKEND_H
