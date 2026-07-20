#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QIcon>

#include "AlarmManager.h"
#include "AudioPlayer.h"
#include "Scheduler.h"
#include "WakeManager.h"
#include "ConfigManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("Lumalarm");
    app.setOrganizationName("Lumalarm");

    QQuickStyle::setStyle("Fusion");

    qmlRegisterUncreatableType<AlarmManager>("GlassAlarm", 1, 0, "AlarmManager", "Singleton");
    qmlRegisterUncreatableType<AudioPlayer>("GlassAlarm", 1, 0, "AudioPlayer", "Singleton");

    AlarmManager alarmManager;
    AudioPlayer audioPlayer;
    ConfigManager configManager;
    WakeManager wakeManager;
    Scheduler scheduler(&alarmManager);
    scheduler.restoreSnoozes();

    // When an alarm triggers: play audio, run command, show overlay
    QObject::connect(&scheduler, &Scheduler::alarmTriggered, [&](int index) {
        const auto alarms = alarmManager.alarmList();
        if (index < 0 || index >= alarms.size()) return;

        const Alarm &a = alarms[index];
        alarmManager.resetSnoozeCount(index);

        // Execute custom command if enabled
        if (a.enableCommand && !a.command.isEmpty()) {
            QProcess::startDetached(a.command);
        }

        // Escalating wake stage 1: brightness ramp via brightnessctl
        if (a.escalatingWake) {
            for (int i = 1; i <= 15; ++i) {
                int pct = qMin(100, i * 100 / 15);
                QTimer::singleShot(i * 1000, [pct]() {
                    QProcess::startDetached("brightnessctl", {"s", QString::number(pct) + "%"});
                });
            }
        }

        // Play alarm sound immediately
        if (a.enableSound) {
            QString sf = a.soundFile;
            int bv = a.baseVolume;
            int fd = a.fadeDuration;
            audioPlayer.setBaseVolume(bv);
            audioPlayer.setFadeDuration(fd);
            if (audioPlayer.isSoundscapePlaying()) {
                audioPlayer.crossfadeToMain(bv, fd);
            } else {
                audioPlayer.play(sf);
            }
        }

        // Auto-stop the alarm after the configured duration
        QTimer::singleShot(a.autoStopDuration * 1000, [&audioPlayer]() {
            if (audioPlayer.isPlaying()) {
                audioPlayer.stop();
            }
        });
    });

    // Soundscape pre-alarm: play ambient track 90s before alarm
    QObject::connect(&scheduler, &Scheduler::soundscapeStarting, [&](int index) {
        const auto alarms = alarmManager.alarmList();
        if (index < 0 || index >= alarms.size()) return;

        const Alarm &a = alarms[index];
        if (!a.soundscape.isEmpty()) {
            audioPlayer.playSoundscape(a.soundscape, 5);
        }
    });

    // Reinitialize audio on system resume (fixes PipeWire/PulseAudio after suspend)
    QObject::connect(&scheduler, &Scheduler::systemResumed, [&audioPlayer]() {
        audioPlayer.reinitializeAudio();
    });

    // Log dismiss events with stage info (feeds future statistics)
    QObject::connect(&alarmManager, &AlarmManager::alarmDismissed, [](int index, int stage) {
        Q_UNUSED(index);
        Q_UNUSED(stage);
        // Future: persist to statistics log
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("alarmManager", &alarmManager);
    engine.rootContext()->setContextProperty("audioPlayer", &audioPlayer);
    engine.rootContext()->setContextProperty("configManager", &configManager);
    engine.rootContext()->setContextProperty("wakeManager", &wakeManager);
    engine.rootContext()->setContextProperty("scheduler", &scheduler);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(QUrl("qrc:/GlassAlarm/qml/main.qml"));

    // Set window icon after QML window is created
    if (!engine.rootObjects().isEmpty()) {
        QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            win->setIcon(QIcon(":/icon.svg"));
        }
    }

    return app.exec();
}
