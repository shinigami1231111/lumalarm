#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

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

        // Play alarm sound (or crossfade from soundscape if active)
        if (a.enableSound) {
            audioPlayer.setBaseVolume(a.baseVolume);
            audioPlayer.setFadeDuration(a.fadeDuration);
            QTimer::singleShot(2500, [&audioPlayer, &a]() {
                if (audioPlayer.isSoundscapePlaying()) {
                    audioPlayer.crossfadeToMain(a.baseVolume, a.fadeDuration);
                } else {
                    audioPlayer.play(a.soundFile);
                }
            });
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

    return app.exec();
}
