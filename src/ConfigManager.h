#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

class ConfigManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int defaultSnooze READ defaultSnooze WRITE setDefaultSnooze NOTIFY configChanged)
    Q_PROPERTY(int defaultFadeDuration READ defaultFadeDuration WRITE setDefaultFadeDuration NOTIFY configChanged)
    Q_PROPERTY(QString defaultWakeMode READ defaultWakeMode WRITE setDefaultWakeMode NOTIFY configChanged)
    Q_PROPERTY(QString tonesDirectory READ tonesDirectory NOTIFY configChanged)
    // Theme properties
    Q_PROPERTY(QString themeBg READ themeBg WRITE setThemeBg NOTIFY configChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent WRITE setThemeAccent NOTIFY configChanged)
    Q_PROPERTY(int themeBlur READ themeBlur WRITE setThemeBlur NOTIFY configChanged)
    Q_PROPERTY(double themeOpacity READ themeOpacity WRITE setThemeOpacity NOTIFY configChanged)
    Q_PROPERTY(double themeCardOpacity READ themeCardOpacity WRITE setThemeCardOpacity NOTIFY configChanged)
    Q_PROPERTY(QString themeTextPrimary READ themeTextPrimary WRITE setThemeTextPrimary NOTIFY configChanged)
    Q_PROPERTY(QString themeTextSecondary READ themeTextSecondary WRITE setThemeTextSecondary NOTIFY configChanged)
    Q_PROPERTY(bool stopwatchShowMs READ stopwatchShowMs WRITE setStopwatchShowMs NOTIFY configChanged)

public:
    explicit ConfigManager(QObject *parent = nullptr);

    int defaultSnooze() const;
    void setDefaultSnooze(int minutes);

    int defaultFadeDuration() const;
    void setDefaultFadeDuration(int seconds);

    QString defaultWakeMode() const;
    void setDefaultWakeMode(const QString &mode);

    QString tonesDirectory() const;

    Q_INVOKABLE QStringList availableTones() const;

    Q_INVOKABLE QString copyToTones(const QString &sourcePath);
    Q_INVOKABLE bool deleteTone(const QString &fileName);

    Q_INVOKABLE QString configFilePath() const;

    // Theme getters/setters
    QString themeBg() const;
    void setThemeBg(const QString &color);

    QString themeAccent() const;
    void setThemeAccent(const QString &color);

    int themeBlur() const;
    void setThemeBlur(int radius);

    double themeOpacity() const;
    void setThemeOpacity(double opacity);

    double themeCardOpacity() const;
    void setThemeCardOpacity(double opacity);

    QString themeTextPrimary() const;
    void setThemeTextPrimary(const QString &color);

    QString themeTextSecondary() const;
    void setThemeTextSecondary(const QString &color);

    bool stopwatchShowMs() const;
    void setStopwatchShowMs(bool show);

signals:
    void configChanged();

private:
    QSettings *m_settings;
    QString m_configDir;
    QString m_tonesDir;
    void migrateOldConfig();
};

#endif // CONFIGMANAGER_H
