#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QColor>
#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QString>

class ThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString background_color READ background_color WRITE set_background_color NOTIFY
                   themeChanged)
    Q_PROPERTY(QString accent_color READ accent_color WRITE set_accent_color NOTIFY themeChanged)
    Q_PROPERTY(QString text_primary READ text_primary WRITE set_text_primary NOTIFY themeChanged)
    Q_PROPERTY(
        QString text_secondary READ text_secondary WRITE set_text_secondary NOTIFY themeChanged)
    Q_PROPERTY(QString border_color READ border_color WRITE set_border_color NOTIFY themeChanged)
    Q_PROPERTY(int blur_radius READ blur_radius WRITE set_blur_radius NOTIFY themeChanged)
    Q_PROPERTY(QString blur_mode READ blur_mode WRITE set_blur_mode NOTIFY themeChanged)
    Q_PROPERTY(QString font_family READ font_family WRITE set_font_family NOTIFY themeChanged)
    Q_PROPERTY(int corner_radius READ corner_radius WRITE set_corner_radius NOTIFY themeChanged)
    Q_PROPERTY(QString background_preset READ background_preset WRITE set_background_preset NOTIFY
                   themeChanged)
    Q_PROPERTY(bool show_minimize_button READ show_minimize_button WRITE set_show_minimize_button
                   NOTIFY themeChanged)
    Q_PROPERTY(bool show_maximize_button READ show_maximize_button WRITE set_show_maximize_button
                   NOTIFY themeChanged)
    Q_PROPERTY(bool show_close_button READ show_close_button WRITE set_show_close_button NOTIFY
                   themeChanged)
    Q_PROPERTY(bool show_tray_icon READ show_tray_icon WRITE set_show_tray_icon NOTIFY themeChanged)
    Q_PROPERTY(
        QString current_preset READ current_preset WRITE set_current_preset NOTIFY themeChanged)

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

    QString font_family() const;
    void set_font_family(const QString &v);

    int corner_radius() const;
    void set_corner_radius(int v);

    QString background_preset() const;
    void set_background_preset(const QString &v);

    bool show_minimize_button() const;
    void set_show_minimize_button(bool v);

    bool show_maximize_button() const;
    void set_show_maximize_button(bool v);

    bool show_close_button() const;
    void set_show_close_button(bool v);

    bool show_tray_icon() const;
    void set_show_tray_icon(bool v);

    QString current_preset() const;
    void set_current_preset(const QString &v);

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
