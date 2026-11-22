#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QDir>
#include <QFile>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>

class ThemeManager;

class ConfigManager : public QObject
{
    Q_OBJECT
    // The rtcwake sleep state — the one mode setting, handed to the helper as
    // `rtcwake -m <mode>`. It used to be shadowed by a `defaultWakeMode`
    // property that nothing consumed; removed2026-10-08.
    Q_PROPERTY(QString sleepMode READ sleepMode WRITE setSleepMode NOTIFY configChanged)
    Q_PROPERTY(
        QString timePickerMode READ timePickerMode WRITE setTimePickerMode NOTIFY configChanged)
    Q_PROPERTY(QString tonesDirectory READ tonesDirectory NOTIFY configChanged)
    // Package-installed, read-only tone folder (the first existing XDG data
    // location carrying lumalarm/tones — /usr/share/lumalarm/tones on an Arch
    // install). Empty when the install ships no shared folder.
    Q_PROPERTY(QString systemTonesDirectory READ systemTonesDirectory NOTIFY configChanged)
    // Machine-readable reason the last tone import/delete failed; "" = success.
    // Codes: "exists" | "source-missing" | "invalid-path" | "unsupported" |
    // "destination" | "permission" | "copy-failed" | "not-found" | "read-only"
    // | "delete-failed". The UI asks the user before an "exists" overwrite —
    // Issue #2's single duplicate policy.
    Q_PROPERTY(QString toneError READ toneError NOTIFY toneErrorChanged)
    // Translated, user-visible form of toneError(). Empty alongside it.
    Q_PROPERTY(QString toneErrorMessage READ toneErrorMessage NOTIFY toneErrorChanged)
    // Theme properties
    Q_PROPERTY(QString themeBg READ themeBg WRITE setThemeBg NOTIFY configChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent WRITE setThemeAccent NOTIFY configChanged)
    Q_PROPERTY(double themeOpacity READ themeOpacity WRITE setThemeOpacity NOTIFY configChanged)
    Q_PROPERTY(QString themeTextPrimary READ themeTextPrimary WRITE setThemeTextPrimary NOTIFY
                   configChanged)
    Q_PROPERTY(QString themeTextSecondary READ themeTextSecondary WRITE setThemeTextSecondary NOTIFY
                   configChanged)
    Q_PROPERTY(
        bool stopwatchShowMs READ stopwatchShowMs WRITE setStopwatchShowMs NOTIFY configChanged)
    // Duration (seconds) offered by the Settings page "Test alarm" card and
    // passed to WakeManager::armTestWake(). Clamped to [60, 3600].
    Q_PROPERTY(
        int testWakeDuration READ testWakeDuration WRITE setTestWakeDuration NOTIFY configChanged)
    // Desktop notifications (freedesktop protocol). On by default.
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled
                   NOTIFY configChanged)
    // System tray icon visibility. Defaults to the pre-existing theme.conf
    // setting (show_tray_icon) so upgrading users keep their choice.
    Q_PROPERTY(
        bool trayIconEnabled READ trayIconEnabled WRITE setTrayIconEnabled NOTIFY configChanged)
    // What the window close button does: "ask" (default), "hide" or "quit".
    // "ask" is the safe default — the first close offers the choice instead of
    // guessing, and the prompt's "don't ask again" box is what settles it.
    Q_PROPERTY(QString closeAction READ closeAction WRITE setCloseAction NOTIFY configChanged)
    // Alarm ringing behaviour when the window is hidden to tray:
    // "alarmOnly" shows only the ringing overlay; "mainAndAlarm" shows both.
    Q_PROPERTY(QString alarmUiMode READ alarmUiMode WRITE setAlarmUiMode NOTIFY configChanged)
    // Night mode: dim the window before an alarm. Defaults are 0 / 30 min / 0.4.
    Q_PROPERTY(
        bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY configChanged)
    Q_PROPERTY(int nightModeMinutesBefore READ nightModeMinutesBefore WRITE
                   setNightModeMinutesBefore NOTIFY configChanged)
    Q_PROPERTY(double nightModeOpacity READ nightModeOpacity WRITE setNightModeOpacity NOTIFY
                   configChanged)
    // First-run wizard (STG-5.5): true once the user finished or skipped it.
    Q_PROPERTY(
        bool wizardCompleted READ wizardCompleted WRITE setWizardCompleted NOTIFY configChanged)
    // UI language (STG-5.2). Empty = follow the system locale; otherwise a Qt
    // locale name ("en", "ar") that matches translations/lumalarm_<lang>.ts.
    // NOTIFY stays configChanged like every other property here; languageChanged
    // is an additional signal main.cpp uses to swap the active QTranslator.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY configChanged)
    // Mirror the whole UI right-to-left. Only offered when Arabic is chosen;
    // off by default so English and Japanese stay left-to-right.
    Q_PROPERTY(
        bool mirrorApp READ mirrorApp WRITE setMirrorApp NOTIFY configChanged)
    // Mirror the title-bar window buttons (quit / minimize / maximize). Only
    // offered when Arabic is chosen; off by default so the buttons stay put.
    Q_PROPERTY(
        bool mirrorWindowButtons READ mirrorWindowButtons WRITE
            setMirrorWindowButtons NOTIFY configChanged)
    // Emoji density: 0 = off, 1 = a few leading icons, 2 = icons on most
    // labels and buttons. Stored as an int so the selector has three states.
    Q_PROPERTY(int emojiLevel READ emojiLevel WRITE setEmojiLevel NOTIFY configChanged)
    // ── audio (alarm sound rebuild, Phase 4) ────────────────────────────────
    // The guarantee policy's knobs, read by main.cpp into
    // AlarmAudioController::applySettings(). Values are validated on read, so
    // a hand-edited settings.ini can never produce an out-of-range level.
    // policy: "guarantee" (default) | "alarmOnly".
    Q_PROPERTY(QString audioPolicy READ audioPolicy WRITE setAudioPolicy NOTIFY
                   configChanged)
    // Guarantee: raise the sink to at least this (0-100) when an alarm rings.
    Q_PROPERTY(int audioFloorPercent READ audioFloorPercent WRITE
                   setAudioFloorPercent NOTIFY configChanged)
    // Our own stream volume (0-100); clamped to 1.0 in the player.
    Q_PROPERTY(int audioStreamPercent READ audioStreamPercent WRITE
                   setAudioStreamPercent NOTIFY configChanged)
    // Global default tone (bare name); "" = fall back to the first tone.
    Q_PROPERTY(QString audioDefaultSound READ audioDefaultSound WRITE
                   setAudioDefaultSound NOTIFY configChanged)
    // Output device for the alarm ring (node.name); "" = follow the system
    // default sink. Stored as a node.name so it survives backend restarts.
    Q_PROPERTY(QString audioOutputSink READ audioOutputSink WRITE
                   setAudioOutputSink NOTIFY configChanged)

public:
    explicit ConfigManager(QObject *parent = nullptr);

    // Selected rtcwake sleep mode. Constrained to the modes this build offers;
    // an unknown stored value reads back as the default rather than being
    // passed to the helper.
    QString sleepMode() const;
    void setSleepMode(const QString &mode);

    // Modes offered by the Settings selector, in display order (standby sits
    // between mem and disk, per the user, 2026-10-08). Single source of truth
    // for the QML model and for the validity check in sleepMode().
    Q_INVOKABLE QStringList sleepModeChoices() const;

    // Alarm-editor time picker style: "wheel" (default) | "spin".
    QString timePickerMode() const;
    void setTimePickerMode(const QString &mode);

    QString tonesDirectory() const;
    QString systemTonesDirectory() const;

    Q_INVOKABLE QStringList availableTones() const;

    // Imports sourcePath into the user tones folder. Refuses to overwrite a
    // tone of the same name: reports toneError() == "exists" and copies
    // nothing, so the UI can ask first. Returns the stored file name on
    // success, an empty string on failure (the reason is always in toneError).
    // sourcePath accepts a "file://" URL string — as QML hands one over — or a
    // plain filesystem path.
    Q_INVOKABLE QString copyToTones(const QString &sourcePath);
    // The same import, explicitly overwriting an existing tone of that name.
    // Reachable from the UI only after the user confirmed the replacement.
    Q_INVOKABLE QString replaceTone(const QString &sourcePath);
    Q_INVOKABLE bool deleteTone(const QString &fileName);
    // Absolute local path -> file URL for a tone, resolved through the same
    // search list as discovery. Null when the tone does not exist.
    Q_INVOKABLE QUrl toneFileUrl(const QString &fileName) const;

    QString toneError() const;
    QString toneErrorMessage() const;

    Q_INVOKABLE QString configFilePath() const;

    // Theme getters/setters (delegated to ThemeManager)
    QString themeBg() const;
    void setThemeBg(const QString &color);

    QString themeAccent() const;
    void setThemeAccent(const QString &color);

    // Retained for compatibility; maps to card_opacity in theme.conf.
    double themeOpacity() const;
    void setThemeOpacity(double opacity);

    QString themeTextPrimary() const;
    void setThemeTextPrimary(const QString &color);

    QString themeTextSecondary() const;
    void setThemeTextSecondary(const QString &color);

    ThemeManager *theme() const;

    bool stopwatchShowMs() const;
    void setStopwatchShowMs(bool show);

    // Stored test-alarm duration in seconds. Defaults to 60 and is clamped to
    // [60, 3600]; invalid or out-of-range stored values read back as 60.
    int testWakeDuration() const;
    void setTestWakeDuration(int seconds);

    bool notificationsEnabled() const;
    void setNotificationsEnabled(bool enabled);

    bool trayIconEnabled() const;
    void setTrayIconEnabled(bool enabled);

    QString closeAction() const;
    void setCloseAction(const QString &action);

    QString alarmUiMode() const;
    void setAlarmUiMode(const QString &mode);

    bool nightModeEnabled() const;
    void setNightModeEnabled(bool enabled);
    int nightModeMinutesBefore() const;
    void setNightModeMinutesBefore(int minutes);
    double nightModeOpacity() const;
    void setNightModeOpacity(double opacity);

    // First-run wizard (STG-5.5). Explicit key == that value. No key: a fresh
    // profile (no settings.ini / theme.conf yet) reads as not completed and
    // opens the wizard; a profile that predates the wizard reads as completed,
    // so an upgrade never force-opens it.
    bool wizardCompleted() const;
    void setWizardCompleted(bool completed);

    // UI language: "" (default) = follow the system locale, otherwise a Qt
    // locale name such as "en" or "ar". Never a display string — the combo box
    // in Settings maps its labels onto these values.
    QString language() const;
    void setLanguage(const QString &lang);

    // Arabic-only UI options, persisted with the language.
    bool mirrorApp() const;
    void setMirrorApp(bool v);
    bool mirrorWindowButtons() const;
    void setMirrorWindowButtons(bool v);
    int emojiLevel() const;
    void setEmojiLevel(int level);
    // QML-facing helper: every decorative emoji goes through this so one
    // setting controls all of them and no literal can bypass the selector.
    Q_INVOKABLE QString emoji(const QString &symbol, int minLevel) const;

    // Audio (Phase 4): policy kind string ("guarantee" | "alarmOnly"; any
    // unknown value reads back as "guarantee"), sink floor %, our stream
    // volume %, and the global default tone name. Phase 6: the output
    // device (speaker selector) as a node.name, "" = system default.
    QString audioPolicy() const;
    void setAudioPolicy(const QString &policy);
    int audioFloorPercent() const;
    void setAudioFloorPercent(int percent);
    int audioStreamPercent() const;
    void setAudioStreamPercent(int percent);
    QString audioDefaultSound() const;
    void setAudioDefaultSound(const QString &fileName);
    QString audioOutputSink() const;
    void setAudioOutputSink(const QString &sinkName);

signals:
    void configChanged();
    // Emitted only by setLanguage(); lets main.cpp reload the catalog without
    // re-running for every unrelated configChanged().
    void languageChanged();
    // Emitted whenever the last tone operation's outcome changes, so the
    // import/delete feedback in SoundManager.qml can never go stale.
    void toneErrorChanged();

private:
    // Shared implementation of copyToTones()/replaceTone().
    QString importTone(const QString &sourcePath, bool replaceExisting);
    // Records the outcome of a tone operation ("" == success) and notifies.
    // detail names the offending file for the messages that need one.
    void setToneError(const QString &code, const QString &detail = QString());

    QSettings *m_settings;
    QString m_configDir;
    QString m_tonesDir;
    QString m_toneError;
    QString m_toneErrorDetail;
    ThemeManager *m_theme;
    // True when this profile carries no evidence of an earlier Lumalarm run;
    // sampled once in the constructor, before anything is created on disk.
    bool m_firstRunProfile = true;
    void migrateOldConfig();
};

#endif // CONFIGMANAGER_H
