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
    // rtcwake is NOT called here because we're already awake at alarm time.
    // Use wakeManager.prepareWake() BEFORE the alarm to suspend+wake.
    QObject::connect(&scheduler, &Scheduler::alarmTriggered, [&](int index) {
        const auto alarms = alarmManager.alarmList();
        if (index < 0 || index >= alarms.size()) return;

        const Alarm &a = alarms[index];

        // Execute custom command if enabled
        if (a.enableCommand && !a.command.isEmpty()) {
            QProcess::startDetached(a.command);
        }

        // Play alarm sound with 2.5s delay after wake for audio reinit
        if (a.enableSound) {
            audioPlayer.setBaseVolume(a.baseVolume);
            audioPlayer.setFadeDuration(a.fadeDuration);
            QTimer::singleShot(2500, [&audioPlayer, soundFile = a.soundFile]() {
                audioPlayer.play(soundFile);
            });
        }

        // Auto-stop the alarm after the configured duration
        QTimer::singleShot(a.autoStopDuration * 1000, [&audioPlayer]() {
            if (audioPlayer.isPlaying()) {
                audioPlayer.stop();
            }
        });
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
