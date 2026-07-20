#include "ConfigManager.h"
#include "ThemeManager.h"
#include <QFileInfoList>
#include <QFileInfo>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    migrateOldConfig();

    m_configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
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

int ConfigManager::defaultSnooze() const
{
    return qMax(1, m_settings->value("alarm/defaultSnooze", 1).toInt());
}

void ConfigManager::setDefaultSnooze(int minutes)
{
    m_settings->setValue("alarm/defaultSnooze", minutes);
    emit configChanged();
}

int ConfigManager::defaultFadeDuration() const
{
    return m_settings->value("alarm/defaultFadeDuration", 15).toInt();
}

void ConfigManager::setDefaultFadeDuration(int seconds)
{
    m_settings->setValue("alarm/defaultFadeDuration", seconds);
    emit configChanged();
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
    return m_theme->card_opacity();
}
void ConfigManager::setThemeOpacity(double opacity)
{
    m_theme->set_card_opacity(opacity);
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

int ConfigManager::timePickerStyle() const
{
    return m_settings->value("ui/timePickerStyle", 0).toInt();
}
void ConfigManager::setTimePickerStyle(int style)
{
    m_settings->setValue("ui/timePickerStyle", qBound(0, style, 2));
    emit configChanged();
}

QString ConfigManager::configFilePath() const
{
    return m_settings->fileName();
}
