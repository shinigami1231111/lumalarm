#include "ConfigManager.h"
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
    return m_settings->value("alarm/defaultSnooze", 5).toInt();
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

    if (QFile::copy(sourcePath, destPath))
        return fileName;

    return QString();
}

bool ConfigManager::deleteTone(const QString &fileName)
{
    if (fileName.isEmpty()) return false;
    QString filePath = m_tonesDir + "/" + fileName;
    return QFile::remove(filePath);
}

QString ConfigManager::themeBg() const
{
    return m_settings->value("theme/bg", "#0d0d1a").toString();
}
void ConfigManager::setThemeBg(const QString &color)
{
    m_settings->setValue("theme/bg", color);
    emit configChanged();
}

QString ConfigManager::themeAccent() const
{
    return m_settings->value("theme/accent", "#3d7fff").toString();
}
void ConfigManager::setThemeAccent(const QString &color)
{
    m_settings->setValue("theme/accent", color);
    emit configChanged();
}

int ConfigManager::themeBlur() const
{
    return m_settings->value("theme/blur", 10).toInt();
}
void ConfigManager::setThemeBlur(int radius)
{
    m_settings->setValue("theme/blur", radius);
    emit configChanged();
}

double ConfigManager::themeOpacity() const
{
    return m_settings->value("theme/opacity", 0.55).toDouble();
}
void ConfigManager::setThemeOpacity(double opacity)
{
    m_settings->setValue("theme/opacity", opacity);
    emit configChanged();
}

double ConfigManager::themeCardOpacity() const
{
    return m_settings->value("theme/cardOpacity", 0.06).toDouble();
}
void ConfigManager::setThemeCardOpacity(double opacity)
{
    m_settings->setValue("theme/cardOpacity", opacity);
    emit configChanged();
}

QString ConfigManager::themeTextPrimary() const
{
    return m_settings->value("theme/textPrimary", "#ffffff").toString();
}
void ConfigManager::setThemeTextPrimary(const QString &color)
{
    m_settings->setValue("theme/textPrimary", color);
    emit configChanged();
}

QString ConfigManager::themeTextSecondary() const
{
    return m_settings->value("theme/textSecondary", "#808090").toString();
}
void ConfigManager::setThemeTextSecondary(const QString &color)
{
    m_settings->setValue("theme/textSecondary", color);
    emit configChanged();
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

QString ConfigManager::configFilePath() const
{
    return m_settings->fileName();
}
