#include "ui/TrayIcon.h"

#include "models/AlarmModel.h"
#include "wake/Scheduler.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QDateTime>
#include <QIcon>
#include <QMenu>

TrayIcon::TrayIcon(AlarmModel *model, Scheduler *scheduler, QObject *parent)
    : QObject(parent)
    , m_tray(new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.svg")), this))
    , m_model(model)
    , m_scheduler(scheduler)
{
    QMenu *menu = new QMenu;
    QAction *open = menu->addAction(tr("Open Lumalarm"));
    menu->addSeparator();
    QAction *quit = menu->addAction(tr("Quit"));
    // The menu is owned by the tray icon, which is owned by this object.
    m_tray->setContextMenu(menu);

    connect(open, &QAction::triggered, this, &TrayIcon::showWindowRequested);
    connect(quit, &QAction::triggered, this, &TrayIcon::quitRequested);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit showWindowRequested();
    });

    // Any change to the alarm list can change which alarm is next.
    if (m_model) {
        connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() { refresh(); });
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]() { refresh(); });
        connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { refresh(); });
        connect(m_model, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            refresh();
        });
    }
    // Arming or clearing a wake changes what the tooltip should say.
    if (m_scheduler) {
        connect(m_scheduler, &Scheduler::wakeStateChanged, this, [this]() { refresh(); });
        connect(m_scheduler, &Scheduler::nextAlarmChanged, this, [this]() { refresh(); });
    }

    // Tray availability is checked AFTER wiring: the tooltip must keep tracking
    // alarms and wake state even where no tray host exists, otherwise the cached
    // text would freeze at whatever it was when the constructor ran.
    m_available = QSystemTrayIcon::isSystemTrayAvailable();
    if (!m_available && !m_warnedUnavailable) {
        qWarning() << "[TrayIcon] no system tray available on this desktop — icon disabled";
        m_warnedUnavailable = true;
    }

    refresh();
}

QString TrayIcon::tooltip() const
{
    return m_tooltip;
}

bool TrayIcon::isAvailable() const
{
    return m_available;
}

void TrayIcon::setVisible(bool visible)
{
    m_visible = visible;
    if (!m_available)
        return;
    m_tray->setVisible(visible);
}

bool TrayIcon::isVisible() const
{
    return m_visible;
}

void TrayIcon::refresh()
{
    m_tooltip = buildTooltip();
    if (!m_available)
        return;
    m_tray->setToolTip(m_tooltip);
}

QString TrayIcon::buildTooltip() const
{
    if (!m_scheduler)
        return tr("No alarm scheduled");

    const int secs = m_scheduler->secondsUntilNextAlarm();
    if (secs <= 0)
        return tr("No alarm scheduled");

    // secondsUntilNextAlarm() returns whole seconds, so now+secs lands up to 59 s
    // before the actual occurrence (23:58:59 for a 23:59 alarm). Add 59 s before
    // formatting so truncating to "HH:mm" recovers the intended minute.
    const QString time = QDateTime::currentDateTime().addSecs(secs + 59).toString(
        QStringLiteral("HH:mm"));
    if (m_scheduler->wakeState() == QLatin1String("armed"))
        return tr("Next alarm: %1 (in %2 min)").arg(time).arg(secs / 60);
    return tr("Next alarm: %1").arg(time);
}
