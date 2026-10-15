#ifndef MPRISPAUSE_H
#define MPRISPAUSE_H

#include <QObject>
#include <QStringList>

// Pause external media players while an alarm rings (guide: MPRIS pause -
// otherwise Spotify & co. simply play over the alarm). Session-bus only,
// best effort: without a bus or without players this is a no-op, and resume
// only touches players WE paused, never ones the user started meanwhile.
//
// Calls use a short per-call timeout (200 ms): typical local DBus answers in
// well under a millisecond, and the worst case (a couple of wedged players)
// stays inside the ring-start budget. Never used on the quit path.
class MprisPause : public QObject
{
    Q_OBJECT

public:
    explicit MprisPause(QObject *parent = nullptr);
    ~MprisPause() override = default;

    // Pure filter: bus names of MPRIS player instances.
    static QStringList playerServices(const QStringList &registeredNames);

    bool sessionAvailable() const;

    // Pause every player currently in "Playback"; remembers who complied.
    // Virtual so tests can substitute a no-op (never touch real players).
    virtual void pauseAll();
    // Play again exactly those we paused; forgets them.
    virtual void resumeAll();

    QStringList pausedPlayers() const { return m_paused; }

private:
    QStringList m_paused;
};

#endif // MPRISPAUSE_H
