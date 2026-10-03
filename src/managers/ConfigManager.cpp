#include "managers/ConfigManager.h"
#include "managers/ThemeManager.h"
#include <QFileInfoList>
#include <QFileInfo>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    migrateOldConfig();

    m_configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";

    // First-run detection (STG-5.5.5), decided before this constructor creates
    // anything. A profile that already carries settings.ini or theme.conf was
    // set up by a Lumalarm release predating the wizard, so an upgrade must not
    // report it as a first run and force the setup open.
    //
    // alarms.json is deliberately NOT part of the test: main() constructs
    // AlarmManager before ConfigManager, and AlarmManager::loadFromFile()
    // creates the file on a brand-new profile.
    m_firstRunProfile = !QFileInfo::exists(m_configDir + "/settings.ini")
                        && !QFileInfo::exists(m_configDir + "/theme.conf");

    QDir().mkpath(m_configDir);

    m_tonesDir = m_configDir + "/tones";
    QDir().mkpath(m_tonesDir);

    QString settingsPath = m_configDir + "/settings.ini";
    m_settings = new QSettings(settingsPath, QSettings::IniFormat, this);

    m_theme = new ThemeManager(this);
    connect(m_theme, &ThemeManager::themeChanged, this, &ConfigManager::configChanged);
}

void ConfigManager::migrateOldConfig()
{
    QString newDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
    QString oldDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/glass-alarm";

    if (QDir(oldDir).exists() && !QDir(newDir).exists()) {
        QDir().rename(oldDir, newDir);
    }
}

QString ConfigManager::defaultWakeMode() const
{
    return m_settings->value("alarm/defaultWakeMode", "mem").toString();
}

void ConfigManager::setDefaultWakeMode(const QString &mode)
{
    m_settings->setValue("alarm/defaultWakeMode", mode);
    emit configChanged();
}

QString ConfigManager::timePickerMode() const
{
    // Normalized on read so a stale/garbage value in settings.ini can never
    // hide both picker controls (the editor only ever shows wheel or spin).
    const QString mode = m_settings->value("alarm/timePickerMode", "wheel").toString();
    return mode == "spin" ? QStringLiteral("spin") : QStringLiteral("wheel");
}

void ConfigManager::setTimePickerMode(const QString &mode)
{
    m_settings->setValue("alarm/timePickerMode",
                         mode == "spin" ? QStringLiteral("spin") : QStringLiteral("wheel"));
    emit configChanged();
}

QString ConfigManager::tonesDirectory() const
{
    return m_tonesDir;
}

QStringList ConfigManager::availableTones() const
{
    QStringList tones;
    QDir dir(m_tonesDir);
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.wav" << "*.mp3" << "*.ogg" << "*.flac",
                                            QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo &fi : files) {
        tones.append(fi.fileName());
    }
    return tones;
}

QString ConfigManager::copyToTones(const QString &sourcePath)
{
    if (sourcePath.isEmpty()) return QString();

    QFileInfo fi(sourcePath);
    QString fileName = fi.fileName();
    QString destPath = m_tonesDir + "/" + fileName;

    if (fi.absolutePath() == m_tonesDir)
        return fileName;

    if (QFile::exists(destPath))
        return fileName;

    if (QFile::copy(sourcePath, destPath)) {
        emit configChanged();
        return fileName;
    }

    return QString();
}

bool ConfigManager::deleteTone(const QString &fileName)
{
    if (fileName.isEmpty()) return false;
    QString filePath = m_tonesDir + "/" + fileName;
    bool ok = QFile::remove(filePath);
    if (ok)
        emit configChanged();
    return ok;
}

QString ConfigManager::themeBg() const
{
    return m_theme->background_color();
}
void ConfigManager::setThemeBg(const QString &color)
{
    m_theme->set_background_color(color);
}

QString ConfigManager::themeAccent() const
{
    return m_theme->accent_color();
}
void ConfigManager::setThemeAccent(const QString &color)
{
    m_theme->set_accent_color(color);
}

double ConfigManager::themeOpacity() const
{
    return m_settings->value("theme/opacity", 0.55).toDouble();
}
void ConfigManager::setThemeOpacity(double opacity)
{
    m_settings->setValue("theme/opacity", qBound(0.0, opacity, 1.0));
    emit configChanged();
}

QString ConfigManager::themeTextPrimary() const
{
    return m_theme->text_primary();
}
void ConfigManager::setThemeTextPrimary(const QString &color)
{
    m_theme->set_text_primary(color);
}

QString ConfigManager::themeTextSecondary() const
{
    return m_theme->text_secondary();
}
void ConfigManager::setThemeTextSecondary(const QString &color)
{
    m_theme->set_text_secondary(color);
}

ThemeManager *ConfigManager::theme() const
{
    return m_theme;
}

bool ConfigManager::stopwatchShowMs() const
{
    return m_settings->value("stopwatch/showMs", true).toBool();
}
void ConfigManager::setStopwatchShowMs(bool show)
{
    m_settings->setValue("stopwatch/showMs", show);
    emit configChanged();
}

int ConfigManager::testWakeDuration() const
{
    // Missing, unparsable or out-of-range values fall back to the 60 s default
    // so the Settings card can always arm a valid test wake.
    const int stored = m_settings->value(QStringLiteral("testWakeDurationSeconds"), 60).toInt();
    return (stored >= 60 && stored <= 3600) ? stored : 60;
}

void ConfigManager::setTestWakeDuration(int seconds)
{
    m_settings->setValue(QStringLiteral("testWakeDurationSeconds"), qBound(60, seconds, 3600));
    emit configChanged();
}

bool ConfigManager::notificationsEnabled() const
{
    return m_settings->value(QStringLiteral("notificationsEnabled"), true).toBool();
}

void ConfigManager::setNotificationsEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("notificationsEnabled"), enabled);
    emit configChanged();
}

bool ConfigManager::trayIconEnabled() const
{
    return m_settings->value(QStringLiteral("trayIconEnabled"),
                             m_theme->show_tray_icon()).toBool();
}

void ConfigManager::setTrayIconEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("trayIconEnabled"), enabled);
    emit configChanged();
}

bool ConfigManager::nightModeEnabled() const
{
    return m_settings->value(QStringLiteral("nightMode/enabled"), false).toBool();
}

void ConfigManager::setNightModeEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("nightMode/enabled"), enabled);
    emit configChanged();
}

int ConfigManager::nightModeMinutesBefore() const
{
    // Valid range 1..180; anything else in the file falls back to 30.
    const int minutes = m_settings->value(QStringLiteral("nightMode/minutesBefore"), 30).toInt();
    return (minutes >= 1 && minutes <= 180) ? minutes : 30;
}

void ConfigManager::setNightModeMinutesBefore(int minutes)
{
    m_settings->setValue(QStringLiteral("nightMode/minutesBefore"),
                         qBound(1, minutes, 180));
    emit configChanged();
}

double ConfigManager::nightModeOpacity() const
{
    // Valid range 0.0..1.0; anything else in the file falls back to 0.4.
    const double opacity = m_settings->value(QStringLiteral("nightMode/dimOpacity"), 0.4).toDouble();
    return (opacity >= 0.0 && opacity <= 1.0) ? opacity : 0.4;
}

void ConfigManager::setNightModeOpacity(double opacity)
{
    m_settings->setValue(QStringLiteral("nightMode/dimOpacity"), qBound(0.0, opacity, 1.0));
    emit configChanged();
}

bool ConfigManager::wizardCompleted() const
{
    // An explicit key always wins, so Finish, Skip and Settings > "Run again"
    // are the only things that can change the answer.
    const QString key = QStringLiteral("wizard/completed");
    if (m_settings->contains(key))
        return m_settings->value(key).toBool();

    // Key absent: only a genuinely new profile counts as "never completed".
    // A profile predating the wizard reads as completed here so an upgrade
    // never force-opens the setup — --show-wizard and Settings > "Run again"
    // still open it explicitly without touching this key.
    return !m_firstRunProfile;
}

void ConfigManager::setWizardCompleted(bool completed)
{
    m_settings->setValue(QStringLiteral("wizard/completed"), completed);
    emit configChanged();
}

QString ConfigManager::language() const
{
    // Missing key = follow the system locale.
    return m_settings->value(QStringLiteral("ui/language"), QString()).toString();
}

void ConfigManager::setLanguage(const QString &lang)
{
    const QString previous = language();
    m_settings->setValue(QStringLiteral("ui/language"), lang);
    emit configChanged();
    if (previous != lang)
        emit languageChanged();
}

QString ConfigManager::configFilePath() const
{
    return m_settings->fileName();
}
