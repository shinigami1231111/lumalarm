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
    Q_PROPERTY(int defaultSnooze READ defaultSnooze WRITE setDefaultSnooze NOTIFY configChanged)
    Q_PROPERTY(int defaultFadeDuration READ defaultFadeDuration WRITE setDefaultFadeDuration NOTIFY configChanged)
    Q_PROPERTY(QString defaultWakeMode READ defaultWakeMode WRITE setDefaultWakeMode NOTIFY configChanged)
    Q_PROPERTY(QString tonesDirectory READ tonesDirectory NOTIFY configChanged)
    // Theme properties
    Q_PROPERTY(QString themeBg READ themeBg WRITE setThemeBg NOTIFY configChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent WRITE setThemeAccent NOTIFY configChanged)
    Q_PROPERTY(double themeOpacity READ themeOpacity WRITE setThemeOpacity NOTIFY configChanged)
    Q_PROPERTY(QString themeTextPrimary READ themeTextPrimary WRITE setThemeTextPrimary NOTIFY configChanged)
    Q_PROPERTY(QString themeTextSecondary READ themeTextSecondary WRITE setThemeTextSecondary NOTIFY configChanged)
    Q_PROPERTY(bool stopwatchShowMs READ stopwatchShowMs WRITE setStopwatchShowMs NOTIFY configChanged)
    Q_PROPERTY(int timePickerStyle READ timePickerStyle WRITE setTimePickerStyle NOTIFY configChanged)

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

    int timePickerStyle() const;
    void setTimePickerStyle(int style);

signals:
    void configChanged();

private:
    QSettings *m_settings;
    QString m_configDir;
    QString m_tonesDir;
    ThemeManager *m_theme;
    void migrateOldConfig();
};

#endif // CONFIGMANAGER_H
