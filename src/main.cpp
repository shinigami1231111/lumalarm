#include <QApplication>
#include <QDir>
#include <QSocketNotifier>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QIcon>
#include <QLocale>
#include <QLockFile>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTranslator>

#include "audio/AlarmAudioController.h"
#include "audio/VolumePolicy.h"
#include "managers/AlarmManager.h"
#include "managers/ConfigManager.h"
#include "managers/ThemeManager.h"
#include "ui/BlurManager.h"
#include "ui/NightMode.h"
#include "ui/Notifier.h"
#include "ui/TrayController.h"
#include "ui/WizardController.h"
#include "app/AppLifecycle.h"
#include "app/SingleInstance.h"
#include "wake/Scheduler.h"
#include "wake/WakeManager.h"

namespace {
// SIGTERM/SIGINT arrive on a signal context — we may not touch Qt from there.
// The handler only writes a byte to a socketpair; a QSocketNotifier on the read
// end runs on the event loop and does the real work. This matters because a
// hidden-to-tray app must still honour "log out / reboot": the close veto must
// never be able to block session end (Phase 5.3).
int g_sigFd[2] = {-1, -1};

void unixSignalHandler(int)
{
    if (g_sigFd[1] >= 0) {
        const char c = 1;
        const auto n = ::write(g_sigFd[1], &c, sizeof(c));
        Q_UNUSED(n);
    }
}
} // namespace

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
    // A tray app must never quit because its window closed or hid — only an
    // explicit quit() may end it. Without this, hiding to tray kills the app
    // the moment the last window disappears.
    app.setQuitOnLastWindowClosed(false);

    // Single-instance protection: prevent a second Lumalarm process from
    // running concurrently. Two instances would both poll alarms, both fire
    // sounds, and both write to the same config files causing corruption.
    //
    // D-Bus first: it can also RAISE the running instance, so double-clicking
    // the launcher while Lumalarm is hidden in the tray shows the window instead
    // of doing nothing. The lock file below stays as the no-session-bus fallback
    // and as the guarantee against concurrent config writes.
    auto *singleInstance = new SingleInstance(&app);
    const bool isPrimary = singleInstance->registerService();
    if (!isPrimary) {
        singleInstance->notifyExisting();
        return 0;
    }

    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
                       "/lumalarm/lumalarm.lock";
    QDir().mkpath(QFileInfo(lockPath).absolutePath());
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0); // never auto-steal; stale locks require manual removal
    if (!lockFile.tryLock(200)) {
        // Another instance already holds the lock. Exit silently.
        return 0;
    }

    QQuickStyle::setStyle("Fusion");

    qmlRegisterUncreatableType<AlarmManager>("GlassAlarm", 1, 0, "AlarmManager", "Singleton");
    qmlRegisterUncreatableType<AlarmAudioController>("GlassAlarm", 1, 0, "AlarmAudioController",
                                                     "Singleton");
    qmlRegisterUncreatableType<ThemeManager>("GlassAlarm", 1, 0, "ThemeManager", "Singleton");

    AlarmManager alarmManager;
    ConfigManager configManager;
    // The whole alarm-sound stack (mixer, policy, journal, player, MPRIS).
    // QML keeps the historical `audioPlayer` context name; the object behind
    // it is this controller with the old AudioPlayer API (play/preview/stop/
    // isPlaying/playbackError).
    AlarmAudioController audio;
    WakeManager wakeManager;
    BlurManager blurManager;
    Scheduler scheduler(&alarmManager, &wakeManager);

    // Audio settings: seed once, then follow config changes (Phase 5 edits
    // them through ConfigManager; the controller re-reads on configChanged).
    const auto audioSettingsFrom = [&configManager]() {
        AlarmAudioController::Settings s;
        s.policy = VolumePolicy::kindFromString(configManager.audioPolicy());
        s.floorPercent = configManager.audioFloorPercent();
        s.streamPercent = configManager.audioStreamPercent();
        s.defaultSound = configManager.audioDefaultSound();
        s.outputSink = configManager.audioOutputSink();
        return s;
    };
    audio.applySettings(audioSettingsFrom());
    QObject::connect(&configManager, &ConfigManager::configChanged, &audio,
                     [&audio, audioSettingsFrom]() {
                         audio.applySettings(audioSettingsFrom());
                     });
    // Backend probe + crash recovery (journal from a crashed previous run).
    audio.start();

    // Desktop notifications (STG-5.3). Seeded from the persisted setting;
    // kept in sync below whenever the setting changes.
    Notifier notifier(configManager.notificationsEnabled());

    // Night mode (STG-5.6): dims the window before the next alarm. Settings come
    // from configManager, the countdown from scheduler — both outlive this object.
    NightMode nightMode(&configManager, &scheduler);
    // Re-evaluated once per scheduler tick; edge-triggered inside NightMode, so
    // this 1 Hz call cannot produce repeated signals.
    QObject::connect(&scheduler, &Scheduler::nextAlarmChanged, &nightMode, &NightMode::evaluate);
    nightMode.evaluate();

    // First-run wizard (STG-5.5). Parsed before the QML engine loads so the
    // overlay's initial visibility is already correct on the first frame.
    // --show-wizard only overrides what is *reported* for this run; it never
    // rewrites wizard/completed. No other CLI flag is accepted.
    const bool forceWizard = app.arguments().contains(QStringLiteral("--show-wizard"));
    WizardController wizard(&configManager, &wakeManager, &audio);
    wizard.setForceShow(forceWizard);

    // ── i18n (STG-5.2) ───────────────────────────────────────────────────────
    // The catalogs are compiled from translations/*.ts and embedded in the
    // binary under /i18n by qt_add_translations. An empty (or missing) language
    // setting means "follow the system locale"; if no catalog matches, the app
    // falls back to the English source strings.
    //
    // Installing / removing a QTranslator makes Qt re-deliver
    // QEvent::LanguageChange to the widgets, but QML needs an explicit
    // engine.retranslate() — that is wired up right after the engine exists.
    QTranslator translator;
    auto applyLanguage = [&app, &translator](const QString &lang) {
        QString effective = lang;
        if (effective.isEmpty())
            effective = QLocale::system().name();

        app.removeTranslator(&translator);
        const bool loaded =
            !effective.isEmpty() && translator.load(QLocale(effective), QStringLiteral("lumalarm"),
                                                    QStringLiteral("_"), QStringLiteral(":/i18n"));
        if (loaded)
            app.installTranslator(&translator);

        // RTL follows the *loaded* catalog, never the raw setting: a system
        // locale without a catalog must stay left-to-right.
        const Qt::LayoutDirection want =
            (loaded && QLocale(effective).textDirection() == Qt::RightToLeft) ? Qt::RightToLeft
                                                                              : Qt::LeftToRight;
        app.setLayoutDirection(want);
    };
    applyLanguage(configManager.language());

    // Track which alarm is currently ringing so auto-stop timers from previous
    // alarms don't accidentally stop a newer alarm's audio.
    QString currentRingingAlarmId;

    // When an alarm triggers: play audio, run command, show overlay
    QObject::connect(&scheduler, &Scheduler::alarmTriggered, [&](int index) {
        const auto alarms = alarmManager.alarmList();
        if (index < 0 || index >= alarms.size())
            return;

        const Alarm &a = alarms[index];

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

        // Ring: the controller resolves the sound (per-alarm -> global
        // default -> first tone -> silent), raises the sink under the
        // guarantee policy with a write-ahead journal, and verifies/forces
        // our own stream volume. Replaces Issue #1's per-alarm volume.
        if (a.enableSound)
            audio.ring(a.soundFile);

        // Auto-stop the alarm after the configured duration. stopRing() is
        // idempotent and restores the sink; the alarmId check keeps a stale
        // timer from stopping a newer alarm's ring.
        QString alarmId = a.id;
        QTimer::singleShot(a.autoStopDuration * 1000,
                           [&audio, &currentRingingAlarmId, alarmId]() {
                               if (currentRingingAlarmId == alarmId)
                                   audio.stopRing();
                           });
    });

    // Reinitialize audio on system resume (fixes PipeWire/PulseAudio after suspend)
    QObject::connect(&scheduler, &Scheduler::systemResumed,
                     [&audio]() { audio.reinitializeAudio(); });

    // Notifications: upcoming-alarm threshold, alarm fired, test wake armed.
    QObject::connect(&scheduler, &Scheduler::alarmApproaching, &notifier,
                     &Notifier::notifyAlarmUpcoming);
    QObject::connect(&scheduler, &Scheduler::alarmTriggered, &notifier, &Notifier::notifyWakeFired);
    QObject::connect(&wakeManager, &WakeManager::testWakeArmed, &notifier,
                     &Notifier::notifyTestWakeArmed);
    // configChanged is the NOTIFY for notificationsEnabled, so re-read it there.
    QObject::connect(&configManager, &ConfigManager::configChanged, &notifier,
                     [&notifier, &configManager]() {
                         notifier.setEnabled(configManager.notificationsEnabled());
                     });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("alarmManager", &alarmManager);
    engine.rootContext()->setContextProperty("audioPlayer", &audio);
    engine.rootContext()->setContextProperty("configManager", &configManager);
    engine.rootContext()->setContextProperty("themeManager", configManager.theme());
    engine.rootContext()->setContextProperty("wakeManager", &wakeManager);
    engine.rootContext()->setContextProperty("scheduler", &scheduler);
    engine.rootContext()->setContextProperty("blurManager", &blurManager);
    engine.rootContext()->setContextProperty("nightMode", &nightMode);
    engine.rootContext()->setContextProperty("wizard", &wizard);

    // Language selector (Settings > Language): swap the catalog and let the
    // engine re-evaluate every qsTr()/tr() binding that is currently on screen.
    QObject::connect(&configManager, &ConfigManager::languageChanged, &engine, [&]() {
        applyLanguage(configManager.language());
        engine.retranslate();
    });

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    // ── APP LIFECYCLE (Phase 1) ────────────────────────────────────────────
    // AppLifecycle is the ONLY thing allowed to change the window's visibility
    // or state (I1). It is parented to `app`, not `engine`, so it outlives the
    // QML engine and the window — the crash this replaced was a stack-local
    // `auto applyBlur = [&]{...}` lambda referenced after its block ended.
    // Created BEFORE engine.load() so QML can bind to it from the first frame.
    auto *lifecycle = new AppLifecycle(&configManager, &blurManager, &app);
    // The audio restore must finish inside quit(), before the tray/window
    // teardown (guide: synchronous, bounded); the journal covers a timeout.
    lifecycle->attachAudio(&audio);

    // Tray: heap, parented to the lifecycle (I6 — explicit teardown, never left
    // to a stack local racing engine destruction).
    auto *tray = new TrayController(alarmManager.model(), &scheduler, lifecycle);
    lifecycle->attachTray(tray);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, lifecycle,
                     [lifecycle]() { lifecycle->quit(); }, Qt::QueuedConnection);

    // A second launch raises us (I4: queued, never on the D-Bus stack).
    QObject::connect(singleInstance, &SingleInstance::activated, lifecycle,
                     &AppLifecycle::showMainWindow, Qt::QueuedConnection);

    // ── SESSION END / SIGTERM (Phase 5.3) ──────────────────────────────────
    // The close veto and setQuitOnLastWindowClosed(false) must never keep the
    // session alive. Logout goes through commitDataRequest; systemctl --user
    // stop / Ctrl-C go through SIGTERM/SIGINT.
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &app,
                     [lifecycle]() { lifecycle->quit(); }, Qt::DirectConnection);

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_sigFd) == 0) {
        auto *notifier = new QSocketNotifier(g_sigFd[0], QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, &app,
                         [lifecycle, notifier](QSocketDescriptor, QSocketNotifier::Type) {
            char c = 0;
            const auto n = ::read(g_sigFd[0], &c, sizeof(c));
            Q_UNUSED(n);
            notifier->setEnabled(false);   // one signal is enough; never re-enter
            lifecycle->quit();
        });
        std::signal(SIGTERM, unixSignalHandler);
        std::signal(SIGINT, unixSignalHandler);
    }

    engine.rootContext()->setContextProperty("lifecycle", lifecycle);

    // QML module resources live under :/qt/qml/<URI>/ (Qt policy QTP0001, NEW
    // since Qt 6.5 — applied via qt_standard_project_setup(REQUIRES 6.5)).
    engine.load(QUrl("qrc:/qt/qml/GlassAlarm/qml/main.qml"));

    if (engine.rootObjects().isEmpty())
        return 1;

    // The window only exists after the engine has loaded; hand it to the
    // lifecycle, which is now the sole owner of its visibility.
    auto *mainWin = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (mainWin)
        mainWin->setIcon(QIcon(":/icon.svg"));
    lifecycle->attachWindow(mainWin);

    // Show unless started hidden for autostart AND a tray is there to come back
    // through (I8). --hidden without a usable tray deliberately shows the window.
    const bool startHidden = app.arguments().contains(QLatin1String("--hidden"));
    lifecycle->start(startHidden);

    // ── PHASE 6 STRESS HOOKS (env-gated, compiled in but inert by default) ─
    if (qEnvironmentVariableIsSet("LUMALARM_STRESS_QUITRACE")) {
        // Self-driving: call quit(), then fire 20 show/quit emissions. Every one
        // must be a no-op and the process must exit 0 — proves I5 (quit is
        // idempotent and every other entry point goes inert) and I6 (the tray's
        // connections are severed by shutdown() before the loop ends).
        QTimer::singleShot(2000, &app, [lifecycle, tray]() {
            qInfo() << "[stress] calling lifecycle->quit()";
            lifecycle->quit();
            for (int i = 0; i < 20; ++i) {
                QTimer::singleShot(i, qApp, [tray, i]() {
                    tray->showRequested();
                    tray->quitRequested();
                    qInfo() << "[stress] emission" << i << "-> must be a no-op";
                });
            }
        });
    }
    if (qEnvironmentVariableIsSet("LUMALARM_STRESS_HIDESHOW")) {
        auto *t = new QTimer(&app);
        int *n = new int(0);
        QObject::connect(t, &QTimer::timeout, &app, [lifecycle, n]() {
            if (*n % 2 == 0) lifecycle->hideMainWindow();
            else             lifecycle->showMainWindow();
            if (++(*n) > 400) { delete n; QTimer::singleShot(0, qApp, &QCoreApplication::quit); }
        });
        t->start(150);
    }
    // ── END STRESS HOOKS ───────────────────────────────────────────────────

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
                    if (fam.contains(family, Qt::CaseInsensitive)) {
                        available = true;
                        break;
                    }
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
