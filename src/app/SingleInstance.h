#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QObject>
#include <QString>

// Second-instance protection for a tray app.
//
// The old guard was a QLockFile that made a second launch exit silently. That
// prevented two tray icons, but it also meant double-clicking the launcher
// while Lumalarm was already running — most likely hidden in the tray — did
// nothing at all, which reads as "the app is broken".
//
// This claims a well-known session-bus name instead. The FIRST instance keeps
// it; a SECOND one fails to claim it, calls Activate() on the running instance
// (which raises the window) and exits. If there is no session bus the caller
// falls back to the lock file, so nothing regresses on a bare TTY.
class SingleInstance : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.shinigami1231111.Lumalarm")

public:
    explicit SingleInstance(QObject *parent = nullptr);

    // true  -> we are the primary instance, keep going.
    // false -> someone else owns the name; notifyExisting() them and exit.
    bool registerService();

    // Ask the running instance to raise its window. Returns false if it could
    // not be reached (then the caller should fall back to the lock file).
    bool notifyExisting();

signals:
    // Emitted when a second instance asks us to come to the front. Connected to
    // AppLifecycle::showMainWindow(), queued, so it never runs on the D-Bus
    // thread's stack.
    void activated();

public slots:
    // Exported over D-Bus. Deliberately the ONLY exported slot: exposing quit()
    // to the session bus would let any peer kill the app.
    void Activate();
};

#endif // SINGLEINSTANCE_H
