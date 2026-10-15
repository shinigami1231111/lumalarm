#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QSystemTrayIcon>
#include <QTimer>

class QAction;
class QMenu;
class AlarmModel;
class Scheduler;

// System tray presence for Lumalarm. Replaces the old TrayIcon, which was a
// STACK LOCAL in main.cpp and computed tray availability once in its
// constructor. Both of those were wrong:
//
//  - Stack local: on quit, QCoreApplication::quit() starts tearing the engine
//    down while this object can still deliver a click, so the Show handler ran
//    against a half-destroyed window. AppLifecycle now owns us and calls
//    shutdown() explicitly and synchronously BEFORE the loop ends.
//  - Computed-once availability: tray hosts appear late at login and vanish
//    when the panel restarts. usable() is now DYNAMIC and a 3 s retry makes the
//    icon appear by itself once a host shows up.
//
// This class never touches a window. It does not know one exists. It only
// emits showRequested()/quitRequested() and lets AppLifecycle decide (I1).
class TrayController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool usable READ usable NOTIFY usableChanged)

public:
    TrayController(AlarmModel *model, Scheduler *scheduler, QObject *parent = nullptr);

    // DYNAMIC: true only when the icon really exists, is visible, and the
    // platform still reports a tray host.
    bool usable() const;

    // Last computed tooltip. Valid with or without a tray host; exposed for
    // tests and logging.
    QString tooltip() const { return m_tooltip; }

    // Recompute now (tests use this instead of waiting for a model signal).
    void refreshNow() { refresh(); }

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Public, idempotent, synchronous. Called by AppLifecycle::shutdownOnce()
    // before the event loop ends (I6). After this, nothing leaves this object.
    void shutdown();

Q_SIGNALS:
    void showRequested();    // idempotent: Trigger and DoubleClick both fire it
    void quitRequested();
    void usableChanged();

private:
    void ensureCreated();
    void retryTick();
    void refresh();
    void updateInfoLine();
    QString buildTooltip() const;

    QPointer<QSystemTrayIcon> m_tray;
    QMenu *m_menu = nullptr;       // NO parent — Qt does not own context menus
    QAction *m_showAction = nullptr;
    QAction *m_infoAction = nullptr;
    QAction *m_quitAction = nullptr;

    AlarmModel *m_model = nullptr;
    Scheduler *m_scheduler = nullptr;

    QTimer m_retry;                // polls for a tray host that appears late
    QString m_tooltip;
    bool m_enabled = false;
    bool m_available = false;
    bool m_down = false;
};

#endif // TRAYCONTROLLER_H
