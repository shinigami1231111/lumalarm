#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QIcon>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QLockFile>
#include <QSystemTrayIcon>
#include <QMenu>

#include "managers/AlarmManager.h"
#include "audio/AudioPlayer.h"
#include "wake/Scheduler.h"
#include "wake/WakeManager.h"
#include "managers/ConfigManager.h"
#include "managers/ThemeManager.h"
#include "ui/BlurManager.h"
#include "ui/Notifier.h"

int main(int argc, char *argv[])
{
    // True window-level transparency: request an ARGB visual with an 8-bit
    // alpha buffer so the Wayland/X11 compositor sees real per-pixel alpha
    // (required for compositor-side blur to work).
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationName("Lumalarm");
    app.setOrganizationName("Lumalarm");

    // Single-instance protection: prevent a second Lumalarm process from
    // running concurrently. Two instances would both poll alarms, both fire
    // sounds, and both write to the same config files causing corruption.
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                       + "/lumalarm/lumalarm.lock";
    QDir().mkpath(QFileInfo(lockPath).absolutePath());
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0); // never auto-steal; stale locks require manual removal
    if (!lockFile.tryLock(200)) {
        // Another instance already holds the lock. Exit silently.
        return 0;
    }

    QQuickStyle::setStyle("Fusion");

    qmlRegisterUncreatableType<AlarmManager>("GlassAlarm", 1, 0, "AlarmManager", "Singleton");
    qmlRegisterUncreatableType<AudioPlayer>("GlassAlarm", 1, 0, "AudioPlayer", "Singleton");
    qmlRegisterUncreatableType<ThemeManager>("GlassAlarm", 1, 0, "ThemeManager", "Singleton");

    AlarmManager alarmManager;
    AudioPlayer audioPlayer;
    ConfigManager configManager;
    WakeManager wakeManager;
    BlurManager blurManager;
    Scheduler scheduler(&alarmManager, &wakeManager);

    // Desktop notifications (STG-5.3). Seeded from the persisted setting;
    // kept in sync below whenever the setting changes.
    Notifier notifier(configManager.notificationsEnabled());

    // Track which alarm is currently ringing so auto-stop timers from previous
    // alarms don't accidentally stop a newer alarm's audio.
    QString currentRingingAlarmId;

    // When an alarm triggers: play audio, run command, show overlay
    QObject::connect(&scheduler, &Scheduler::alarmTriggered, [&](int index) {
        const auto alarms = alarmManager.alarmList();
        if (index < 0 || index >= alarms.size()) return;

        const Alarm &a = alarms[index];

        // Stop any previously playing alarm audio — only one alarm rings at a time.
        if (audioPlayer.isPlaying()) {
            audioPlayer.stop();
        }

        currentRingingAlarmId = a.id;

        // Execute custom command if enabled.
        // NOTE: startDetached(string) invokes /bin/sh -c on Unix. This is
        // intentional — the custom command feature is designed to support
        // arbitrary shell commands (pipes, redirects, etc.).
        if (a.enableCommand && !a.command.isEmpty()) {
            if (!QProcess::startDetached(a.command)) {
                qWarning() << "Lumalarm: failed to execute command:" << a.command;
            }
        }

        // Play alarm sound immediately
        if (a.enableSound) {
            audioPlayer.play(a.soundFile);
        }

        // Auto-stop the alarm after the configured duration.
        // Capture alarmId by value so the lambda can check if this is still
        // the current alarm — prevents a stale timer from stopping a newer one.
        QString alarmId = a.id;
        QTimer::singleShot(a.autoStopDuration * 1000, [&audioPlayer, &currentRingingAlarmId, alarmId]() {
            if (currentRingingAlarmId == alarmId && audioPlayer.isPlaying()) {
                audioPlayer.stop();
            }
        });
    });

    // Reinitialize audio on system resume (fixes PipeWire/PulseAudio after suspend)
    QObject::connect(&scheduler, &Scheduler::systemResumed, [&audioPlayer]() {
        audioPlayer.reinitializeAudio();
    });

    // Notifications: upcoming-alarm threshold, alarm fired, test wake armed.
    QObject::connect(&scheduler, &Scheduler::alarmApproaching,
                     &notifier, &Notifier::notifyAlarmUpcoming);
    QObject::connect(&scheduler, &Scheduler::alarmTriggered,
                     &notifier, &Notifier::notifyWakeFired);
    QObject::connect(&wakeManager, &WakeManager::testWakeArmed,
                     &notifier, &Notifier::notifyTestWakeArmed);
    // configChanged is the NOTIFY for notificationsEnabled, so re-read it there.
    QObject::connect(&configManager, &ConfigManager::configChanged,
                     &notifier, [&notifier, &configManager]() {
        notifier.setEnabled(configManager.notificationsEnabled());
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("alarmManager", &alarmManager);
    engine.rootContext()->setContextProperty("audioPlayer", &audioPlayer);
    engine.rootContext()->setContextProperty("configManager", &configManager);
    engine.rootContext()->setContextProperty("themeManager", configManager.theme());
    engine.rootContext()->setContextProperty("wakeManager", &wakeManager);
    engine.rootContext()->setContextProperty("scheduler", &scheduler);
    engine.rootContext()->setContextProperty("blurManager", &blurManager);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // QML module resources live under :/qt/qml/<URI>/ (Qt policy QTP0001, NEW
    // since Qt 6.5 — applied via qt_standard_project_setup(REQUIRES 6.5)).
    engine.load(QUrl("qrc:/qt/qml/GlassAlarm/qml/main.qml"));

    // Set window icon after QML window is created
    QQuickWindow *mainWin = nullptr;
    if (!engine.rootObjects().isEmpty()) {
        mainWin = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (mainWin) {
            mainWin->setIcon(QIcon(":/icon.svg"));

            // KWin blur-behind must be requested after the window exists.
            auto applyBlur = [&]() {
                bool compositor = configManager.theme()->blur_mode() == "compositor";
                blurManager.setBlurEnabled(mainWin, compositor);
            };
            // Apply once shown, on every resize, and when blur mode changes.
            QObject::connect(mainWin, &QWindow::visibleChanged, [&](bool vis) {
                if (vis) applyBlur();
            });
            QObject::connect(mainWin, &QWindow::xChanged, applyBlur);
            QObject::connect(mainWin, &QWindow::yChanged, applyBlur);
            QObject::connect(mainWin, &QWindow::widthChanged, applyBlur);
            QObject::connect(mainWin, &QWindow::heightChanged, applyBlur);
            QObject::connect(configManager.theme(), &ThemeManager::themeChanged, applyBlur);
            applyBlur();
        }
    }

    // System tray icon — created once, shown/hidden based on setting.
    auto showWindowFromTray = [&]() {
        if (mainWin) {
            mainWin->setVisible(true);
            mainWin->showNormal();
            mainWin->raise();
            mainWin->requestActivate();
        }
    };

    QSystemTrayIcon trayIcon(QIcon(":/icon.svg"), &app);
    QMenu trayMenu;
    trayMenu.addAction("Show Lumalarm", showWindowFromTray);
    trayMenu.addSeparator();
    trayMenu.addAction("Exit", [&]() { app.quit(); });
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.setToolTip("Lumalarm");

    // Left-click on tray icon: show window directly.
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showWindowFromTray();
    });

    // Show or hide tray icon based on setting. Re-reads on every theme change.
    auto updateTrayVisibility = [&]() {
        if (configManager.theme()->show_tray_icon())
            trayIcon.show();
        else
            trayIcon.hide();
    };
    updateTrayVisibility();
    QObject::connect(configManager.theme(), &ThemeManager::themeChanged, updateTrayVisibility);

    // Apply the theme's font family if it resolves to an installed font.
    // Falls back silently to the system default when unavailable.
    {
        QString family = configManager.theme()->font_family().trimmed();
        if (!family.isEmpty()) {
            QFontDatabase db;
            // Accept if the family is installed (case-insensitive); otherwise
            // silently keep the system default — never crash on a missing font.
            bool available = db.families().contains(family, Qt::CaseInsensitive);
            if (!available) {
                for (const QString &fam : db.families()) {
                    if (fam.contains(family, Qt::CaseInsensitive)) { available = true; break; }
                }
            }
            if (available) {
                QFont f(family);
                app.setFont(f);
            }
        }
    }

    return app.exec();
}
