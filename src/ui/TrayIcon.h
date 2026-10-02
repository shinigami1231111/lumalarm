#ifndef TRAYICON_H
#define TRAYICON_H

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

class AlarmModel;
class Scheduler;

// System tray presence for Lumalarm: a tray icon whose tooltip always names the
// next alarm, with a small menu to raise the window or quit the app.
//
// Ownership: the QSystemTrayIcon is a child of this object, and this object is a
// stack local in main.cpp. AlarmModel and Scheduler are NOT owned — they are only
// observed (alarm-list edits and wake-state changes refresh the tooltip).
//
// On a desktop without a system tray the icon is disabled after a single
// warning and every method degrades to a no-op; nothing here can crash or block.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    TrayIcon(AlarmModel *model, Scheduler *scheduler, QObject *parent = nullptr);

    // Tooltip text currently displayed. Exposed for tests and logging.
    QString tooltip() const;

    // False when the platform has no usable system tray.
    bool isAvailable() const;

    void setVisible(bool visible);
    bool isVisible() const;

Q_SIGNALS:
    // Emitted when the user picks "Open Lumalarm" in the tray menu.
    void showWindowRequested();
    // Emitted when the user picks "Quit" in the tray menu.
    void quitRequested();

private:
    void refresh();
    QString buildTooltip() const;

    QString m_tooltip;   // last computed tooltip, valid with or without a tray
    QSystemTrayIcon *m_tray;
    AlarmModel *m_model;
    Scheduler *m_scheduler;
    bool m_available = false;
    bool m_visible = false;
    bool m_warnedUnavailable = false;
};

#endif // TRAYICON_H
