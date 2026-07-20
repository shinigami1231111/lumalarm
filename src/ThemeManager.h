#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QMap>
#include <QString>
#include <QColor>

class ThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString background_color READ background_color WRITE set_background_color NOTIFY themeChanged)
    Q_PROPERTY(QString accent_color READ accent_color WRITE set_accent_color NOTIFY themeChanged)
    Q_PROPERTY(QString text_primary READ text_primary WRITE set_text_primary NOTIFY themeChanged)
    Q_PROPERTY(QString text_secondary READ text_secondary WRITE set_text_secondary NOTIFY themeChanged)
    Q_PROPERTY(QString border_color READ border_color WRITE set_border_color NOTIFY themeChanged)
    Q_PROPERTY(int blur_radius READ blur_radius WRITE set_blur_radius NOTIFY themeChanged)
    Q_PROPERTY(QString blur_mode READ blur_mode WRITE set_blur_mode NOTIFY themeChanged)
    Q_PROPERTY(double card_opacity READ card_opacity WRITE set_card_opacity NOTIFY themeChanged)
    Q_PROPERTY(QString font_family READ font_family WRITE set_font_family NOTIFY themeChanged)
    Q_PROPERTY(int corner_radius READ corner_radius WRITE set_corner_radius NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QString background_color() const;
    void set_background_color(const QString &v);

    QString accent_color() const;
    void set_accent_color(const QString &v);

    QString text_primary() const;
    void set_text_primary(const QString &v);

    QString text_secondary() const;
    void set_text_secondary(const QString &v);

    QString border_color() const;
    void set_border_color(const QString &v);

    int blur_radius() const;
    void set_blur_radius(int v);

    // "compositor" or "app"
    QString blur_mode() const;
    void set_blur_mode(const QString &v);

    double card_opacity() const;
    void set_card_opacity(double v);

    QString font_family() const;
    void set_font_family(const QString &v);

    int corner_radius() const;
    void set_corner_radius(int v);

    Q_INVOKABLE QString configPath() const;

    // Apply a built-in palette preset (overwrites theme.conf).
    // Names: "Catppuccin Mocha", "Nord", "Gruvbox Dark", "Rose Pine", "Tokyo Night"
    Q_INVOKABLE bool applyPreset(const QString &name);
    Q_INVOKABLE QStringList presetNames() const;

    // Import colors from pywal (~/.cache/wal/colors.json). Manual trigger only.
    Q_INVOKABLE bool importFromPywal();
    Q_INVOKABLE bool pywalAvailable() const;

    // Force a reload from disk (also happens automatically via the watcher).
    Q_INVOKABLE void reload();

    // Convenience: a QColor with alpha applied to the background for the true
    // transparent window surface. Used by main.qml so compositor blur sees real alpha.
    Q_INVOKABLE QColor backgroundWithAlpha() const;

signals:
    void themeChanged();

private:
    void ensureFile();
    void loadFromDisk();
    void writeToDisk();
    void setValue(const QString &key, const QString &value);
    QString value(const QString &key, const QString &fallback) const;

    QString m_filePath;
    QMap<QString, QString> m_theme;
    QFileSystemWatcher *m_watcher;
    bool m_suppressWrite = false;
};

#endif // THEMEMANAGER_H
