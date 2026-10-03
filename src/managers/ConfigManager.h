#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

class ThemeManager;

class ConfigManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString defaultWakeMode READ defaultWakeMode WRITE setDefaultWakeMode NOTIFY configChanged)
    Q_PROPERTY(QString timePickerMode READ timePickerMode WRITE setTimePickerMode NOTIFY configChanged)
    Q_PROPERTY(QString tonesDirectory READ tonesDirectory NOTIFY configChanged)
    // Theme properties
    Q_PROPERTY(QString themeBg READ themeBg WRITE setThemeBg NOTIFY configChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent WRITE setThemeAccent NOTIFY configChanged)
    Q_PROPERTY(double themeOpacity READ themeOpacity WRITE setThemeOpacity NOTIFY configChanged)
    Q_PROPERTY(QString themeTextPrimary READ themeTextPrimary WRITE setThemeTextPrimary NOTIFY configChanged)
    Q_PROPERTY(QString themeTextSecondary READ themeTextSecondary WRITE setThemeTextSecondary NOTIFY configChanged)
    Q_PROPERTY(bool stopwatchShowMs READ stopwatchShowMs WRITE setStopwatchShowMs NOTIFY configChanged)
    // Duration (seconds) offered by the Settings page "Test alarm" card and
    // passed to WakeManager::armTestWake(). Clamped to [60, 3600].
    Q_PROPERTY(int testWakeDuration READ testWakeDuration WRITE setTestWakeDuration NOTIFY configChanged)
    // Desktop notifications (freedesktop protocol). On by default.
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY configChanged)
    // System tray icon visibility. Defaults to the pre-existing theme.conf
    // setting (show_tray_icon) so upgrading users keep their choice.
    Q_PROPERTY(bool trayIconEnabled READ trayIconEnabled WRITE setTrayIconEnabled NOTIFY configChanged)
    // Night mode: dim the window before an alarm. Defaults are 0 / 30 min / 0.4.
    Q_PROPERTY(bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY configChanged)
    Q_PROPERTY(int nightModeMinutesBefore READ nightModeMinutesBefore WRITE setNightModeMinutesBefore NOTIFY configChanged)
    Q_PROPERTY(double nightModeOpacity READ nightModeOpacity WRITE setNightModeOpacity NOTIFY configChanged)
    // First-run wizard (STG-5.5): true once the user finished or skipped it.
    Q_PROPERTY(bool wizardCompleted READ wizardCompleted WRITE setWizardCompleted NOTIFY configChanged)
    // UI language (STG-5.2). Empty = follow the system locale; otherwise a Qt
    // locale name ("en", "ar") that matches translations/lumalarm_<lang>.ts.
    // NOTIFY stays configChanged like every other property here; languageChanged
    // is an additional signal main.cpp uses to swap the active QTranslator.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY configChanged)

public:
    explicit ConfigManager(QObject *parent = nullptr);

    QString defaultWakeMode() const;
    void setDefaultWakeMode(const QString &mode);

    // Alarm-editor time picker style: "wheel" (default) | "spin".
    QString timePickerMode() const;
    void setTimePickerMode(const QString &mode);

    QString tonesDirectory() const;

    Q_INVOKABLE QStringList availableTones() const;

    Q_INVOKABLE QString copyToTones(const QString &sourcePath);
    Q_INVOKABLE bool deleteTone(const QString &fileName);

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

    bool nightModeEnabled() const;
    void setNightModeEnabled(bool enabled);
    int nightModeMinutesBefore() const;
    void setNightModeMinutesBefore(int minutes);
    double nightModeOpacity() const;
    void setNightModeOpacity(double opacity);

    // First-run wizard (STG-5.5). Absent key == "this installation has never
    // completed the guided setup", so a fresh profile opens the wizard.
    bool wizardCompleted() const;
    void setWizardCompleted(bool completed);

    // UI language: "" (default) = follow the system locale, otherwise a Qt
    // locale name such as "en" or "ar". Never a display string — the combo box
    // in Settings maps its labels onto these values.
    QString language() const;
    void setLanguage(const QString &lang);

signals:
    void configChanged();
    // Emitted only by setLanguage(); lets main.cpp reload the catalog without
    // re-running for every unrelated configChanged().
    void languageChanged();

private:
    QSettings *m_settings;
    QString m_configDir;
    QString m_tonesDir;
    ThemeManager *m_theme;
    void migrateOldConfig();
};

#endif // CONFIGMANAGER_H
