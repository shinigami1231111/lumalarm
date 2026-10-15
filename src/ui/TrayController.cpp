#include "ui/TrayController.h"

#include "models/AlarmModel.h"
#include "wake/Scheduler.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QDateTime>
#include <QIcon>
#include <QMenu>
#include <QtLogging>

TrayController::TrayController(AlarmModel *model, Scheduler *scheduler, QObject *parent)
    : QObject(parent), m_model(model), m_scheduler(scheduler)
{
    // Poll for a tray host that has not started yet (login ordering, panel
    // restart). Stopped on shutdown and once the icon is usable.
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, &TrayController::retryTick);

    // Observe only. Model/scheduler outlive us; `this` is the context so these
    // connections vanish with us (I3) — no [&] anywhere in this file.
    if (m_model) {
        connect(m_model, &QAbstractItemModel::rowsInserted, this,
                [this]() { refresh(); });
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]() { refresh(); });
        connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { refresh(); });
        connect(m_model, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
                    refresh();
                });
    }
    if (m_scheduler) {
        connect(m_scheduler, &Scheduler::wakeStateChanged, this, [this]() { refresh(); });
        connect(m_scheduler, &Scheduler::nextAlarmChanged, this, [this]() { refresh(); });
    }

    // Compute the tooltip immediately so it is meaningful before the tray is
    // ever enabled (the tooltip tracks alarms even with no tray host).
    refresh();
}

bool TrayController::usable() const
{
    // DYNAMIC on purpose: a panel restart must be able to make the icon vanish
    // and come back, and "computed once at startup" cannot express that.
    return !m_down && m_tray && m_tray->isVisible()
        && QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::setEnabled(bool enabled)
{
    if (m_down || m_enabled == enabled)
        return;
    m_enabled = enabled;

    if (enabled) {
        ensureCreated();
        m_retry.start();
    } else {
        m_retry.stop();
        if (m_tray)
            m_tray->setVisible(false);
    }
    emit usableChanged();
}

void TrayController::ensureCreated()
{
    if (m_down || m_tray)
        return;                                       // created once, then toggled

    m_available = QSystemTrayIcon::isSystemTrayAvailable();
    if (!m_available) {
        qInfo() << "[TrayController] no tray host yet — will retry every 3 s";
        return;                                       // retryTick() will come back
    }

    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.svg")), this);

    // Qt does NOT take ownership of a context menu, so it is ours and is deleted
    // explicitly in shutdown(), AFTER the icon (I6).
    m_menu = new QMenu;
    m_showAction = m_menu->addAction(tr("Show Lumalarm"));
    m_infoAction = m_menu->addAction(QString());
    m_infoAction->setEnabled(false);                  // info line, not a command
    m_menu->addSeparator();
    m_quitAction = m_menu->addAction(tr("Quit"));
    m_tray->setContextMenu(m_menu);

    connect(m_showAction, &QAction::triggered, this, [this](bool) {
        if (m_down) return;
        emit showRequested();
    });
    connect(m_quitAction, &QAction::triggered, this, [this](bool) {
        if (m_down) return;
        emit quitRequested();
    });
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (m_down) return;
                // Trigger and DoubleClick both mean "show me". Different tray
                // hosts disagree about which one they send, so accept both;
                // showRequested() is idempotent upstream.
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick)
                    emit showRequested();
            });

    m_tray->setVisible(m_enabled);
    refresh();
    emit usableChanged();
}

void TrayController::retryTick()
{
    if (m_down || !m_enabled)
        return;
    if (!m_tray) {
        ensureCreated();
        if (usable())
            m_retry.stop();                          // host appeared — done polling
    } else if (!usable()) {
        emit usableChanged();                         // host went away (panel restart)
    }
}

void TrayController::shutdown()
{
    if (m_down)
        return;
    m_down = true;
    m_retry.stop();

    // Nothing can leave this object any more: drop every connection, including
    // the model/scheduler observers and the QAction handlers.
    disconnect(this, nullptr, nullptr, nullptr);

    if (m_tray) {
        m_tray->hide();                               // icon first
        m_tray->setContextMenu(nullptr);
        m_tray->disconnect();
        delete m_tray;
        m_tray.clear();
    }
    delete m_menu;                                    // menu AFTER the icon
    m_menu = nullptr;
    m_showAction = m_infoAction = m_quitAction = nullptr;

    emit usableChanged();
}

void TrayController::refresh()
{
    if (m_down)
        return;
    m_tooltip = buildTooltip();
    updateInfoLine();
    if (m_tray)
        m_tray->setToolTip(m_tooltip);
}

void TrayController::updateInfoLine()
{
    if (!m_infoAction)
        return;
    // The info line mirrors the tooltip so right-click shows the next alarm too.
    m_infoAction->setText(m_tooltip);
}

QString TrayController::buildTooltip() const
{
    if (!m_scheduler)
        return tr("No alarm scheduled");

    const int secs = m_scheduler->secondsUntilNextAlarm();
    if (secs <= 0)
        return tr("No alarm scheduled");

    // secondsUntilNextAlarm() returns whole seconds, so now+secs lands up to 59 s
    // before the actual occurrence (23:58:59 for a 23:59 alarm). Add 59 s before
    // formatting so truncating to "HH:mm" recovers the intended minute.
    const QString time =
        QDateTime::currentDateTime().addSecs(secs + 59).toString(QStringLiteral("HH:mm"));
    if (m_scheduler->wakeState() == QLatin1String("armed"))
        return tr("Next alarm: %1 (in %2 min)").arg(time).arg(secs / 60);
    return tr("Next alarm: %1").arg(time);
}
