#include "ThemeManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFontDatabase>
#include <QFont>

namespace {
    const QString KEY_BG = "background_color";
    const QString KEY_ACCENT = "accent_color";
    const QString KEY_TEXT1 = "text_primary";
    const QString KEY_TEXT2 = "text_secondary";
    const QString KEY_BORDER = "border_color";
    const QString KEY_BLUR_R = "blur_radius";
    const QString KEY_BLUR_M = "blur_mode";
    const QString KEY_CARD_OP = "card_opacity";
    const QString KEY_FONT = "font_family";
    const QString KEY_RADIUS = "corner_radius";

    QString defaultOf(const QString &key)
    {
        static const QMap<QString, QString> d = {
            {KEY_BG, "#0d0d1a"},
            {KEY_ACCENT, "#3d7fff"},
            {KEY_TEXT1, "#ffffff"},
            {KEY_TEXT2, "#808090"},
            {KEY_BORDER, "#3d7fff"},
            {KEY_BLUR_R, "20"},
            {KEY_BLUR_M, "compositor"},
            {KEY_CARD_OP, "0.55"},
            {KEY_FONT, ""},
            {KEY_RADIUS, "18"}
        };
        return d.value(key, "");
    }

    // Built-in palettes. Backgrounds are dark with modest opacity so the
    // compositor blur shows through; values are "#rrggbb".
    struct Palette { QString bg, accent, text1, text2, border; };
    QMap<QString, Palette> palettes()
    {
        QMap<QString, Palette> p;
        p["Catppuccin Mocha"] = {"#1e1e2e", "#cba6f7", "#cdd6f4", "#a6adc8", "#cba6f7"};
        p["Nord"]             = {"#2e3440", "#88c0d0", "#eceff4", "#d8dee9", "#88c0d0"};
        p["Gruvbox Dark"]     = {"#282828", "#fabd2f", "#ebdbb2", "#a89984", "#fabd2f"};
        p["Rose Pine"]        = {"#191724", "#ebbcba", "#e0def4", "#908caa", "#ebbcba"};
        p["Tokyo Night"]      = {"#1a1b26", "#7aa2f7", "#c0caf5", "#565f89", "#7aa2f7"};
        return p;
    }
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
    QDir().mkpath(configDir);
    m_filePath = configDir + "/theme.conf";

    ensureFile();
    loadFromDisk();

    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(m_filePath);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        // Some editors replace the file; re-add the watch if needed.
        loadFromDisk();
        if (!m_watcher->files().contains(m_filePath))
            m_watcher->addPath(m_filePath);
    });
}

void ThemeManager::ensureFile()
{
    if (!QFile::exists(m_filePath)) {
        // Seed with defaults.
        for (const QString &k : {KEY_BG, KEY_ACCENT, KEY_TEXT1, KEY_TEXT2,
                                  KEY_BORDER, KEY_BLUR_R, KEY_BLUR_M,
                                  KEY_CARD_OP, KEY_FONT, KEY_RADIUS})
            m_theme[k] = defaultOf(k);
        writeToDisk();
    }
}

void ThemeManager::loadFromDisk()
{
    m_suppressWrite = true;
    QFile f(m_filePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#') || line.startsWith('['))
                continue;
            int eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            QString key = line.left(eq).trimmed();
            QString val = line.mid(eq + 1).trimmed();
            m_theme[key] = val;
        }
        f.close();
    }
    // Fill any missing keys with defaults.
    for (const QString &k : {KEY_BG, KEY_ACCENT, KEY_TEXT1, KEY_TEXT2,
                             KEY_BORDER, KEY_BLUR_R, KEY_BLUR_M,
                             KEY_CARD_OP, KEY_FONT, KEY_RADIUS})
        if (!m_theme.contains(k) || m_theme[k].isEmpty())
            m_theme[k] = defaultOf(k);

    m_suppressWrite = false;
    emit themeChanged();
}

void ThemeManager::writeToDisk()
{
    if (m_suppressWrite)
        return;
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    QTextStream out(&f);
    out << "# Lumalarm theme — edit by hand or via Settings. Changes apply live.\n";
    out << "# Blur mode: compositor (let Hyprland/Sway blur the desktop behind) or app (internal blur).\n";
    out << KEY_BG << "=" << m_theme.value(KEY_BG) << "\n";
    out << KEY_ACCENT << "=" << m_theme.value(KEY_ACCENT) << "\n";
    out << KEY_TEXT1 << "=" << m_theme.value(KEY_TEXT1) << "\n";
    out << KEY_TEXT2 << "=" << m_theme.value(KEY_TEXT2) << "\n";
    out << KEY_BORDER << "=" << m_theme.value(KEY_BORDER) << "\n";
    out << KEY_BLUR_R << "=" << m_theme.value(KEY_BLUR_R) << "\n";
    out << KEY_BLUR_M << "=" << m_theme.value(KEY_BLUR_M) << "\n";
    out << KEY_CARD_OP << "=" << m_theme.value(KEY_CARD_OP) << "\n";
    out << KEY_FONT << "=" << m_theme.value(KEY_FONT) << "\n";
    out << KEY_RADIUS << "=" << m_theme.value(KEY_RADIUS) << "\n";
}

void ThemeManager::setValue(const QString &key, const QString &value)
{
    if (m_theme.value(key) == value)
        return;
    m_theme[key] = value;
    writeToDisk();
    emit themeChanged();
}

QString ThemeManager::value(const QString &key, const QString &fallback) const
{
    return m_theme.value(key, fallback);
}

QString ThemeManager::configPath() const { return m_filePath; }

QString ThemeManager::background_color() const { return value(KEY_BG, defaultOf(KEY_BG)); }
void ThemeManager::set_background_color(const QString &v) { setValue(KEY_BG, v); }

QString ThemeManager::accent_color() const { return value(KEY_ACCENT, defaultOf(KEY_ACCENT)); }
void ThemeManager::set_accent_color(const QString &v) { setValue(KEY_ACCENT, v); }

QString ThemeManager::text_primary() const { return value(KEY_TEXT1, defaultOf(KEY_TEXT1)); }
void ThemeManager::set_text_primary(const QString &v) { setValue(KEY_TEXT1, v); }

QString ThemeManager::text_secondary() const { return value(KEY_TEXT2, defaultOf(KEY_TEXT2)); }
void ThemeManager::set_text_secondary(const QString &v) { setValue(KEY_TEXT2, v); }

QString ThemeManager::border_color() const { return value(KEY_BORDER, defaultOf(KEY_BORDER)); }
void ThemeManager::set_border_color(const QString &v) { setValue(KEY_BORDER, v); }

int ThemeManager::blur_radius() const { return value(KEY_BLUR_R, defaultOf(KEY_BLUR_R)).toInt(); }
void ThemeManager::set_blur_radius(int v) { setValue(KEY_BLUR_R, QString::number(v)); }

QString ThemeManager::blur_mode() const
{
    QString m = value(KEY_BLUR_M, defaultOf(KEY_BLUR_M)).toLower();
    return (m == "app") ? "app" : "compositor";
}
void ThemeManager::set_blur_mode(const QString &v)
{
    setValue(KEY_BLUR_M, (v.toLower() == "app") ? "app" : "compositor");
}

double ThemeManager::card_opacity() const { return value(KEY_CARD_OP, defaultOf(KEY_CARD_OP)).toDouble(); }
void ThemeManager::set_card_opacity(double v) { setValue(KEY_CARD_OP, QString::number(qBound(0.0, v, 1.0))); }

QString ThemeManager::font_family() const { return value(KEY_FONT, defaultOf(KEY_FONT)); }
void ThemeManager::set_font_family(const QString &v) { setValue(KEY_FONT, v); }

int ThemeManager::corner_radius() const { return value(KEY_RADIUS, defaultOf(KEY_RADIUS)).toInt(); }
void ThemeManager::set_corner_radius(int v) { setValue(KEY_RADIUS, QString::number(qMax(0, v))); }

QColor ThemeManager::backgroundWithAlpha() const
{
    QColor c(background_color());
    if (!c.isValid())
        c = QColor(defaultOf(KEY_BG));
    double alpha = card_opacity();
    // In compositor blur mode we want the window surface to be genuinely
    // transparent so the compositor's own blur shows the desktop through.
    // We keep a little tint but rely on real per-pixel alpha, not QML opacity.
    c.setAlphaF(qBound(0.0, alpha, 1.0));
    return c;
}

bool ThemeManager::applyPreset(const QString &name)
{
    auto p = palettes();
    if (!p.contains(name))
        return false;
    Palette pal = p[name];
    m_suppressWrite = false;
    set_background_color(pal.bg);
    set_accent_color(pal.accent);
    set_text_primary(pal.text1);
    set_text_secondary(pal.text2);
    set_border_color(pal.border);
    emit themeChanged();
    return true;
}

QStringList ThemeManager::presetNames() const
{
    return palettes().keys();
}

bool ThemeManager::pywalAvailable() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    // ~/.cache/wal/colors.json
    QString wal = QDir::homePath() + "/.cache/wal/colors.json";
    return QFile::exists(wal);
}

bool ThemeManager::importFromPywal()
{
    QString wal = QDir::homePath() + "/.cache/wal/colors.json";
    QFile f(wal);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;
    QJsonObject o = doc.object();

    auto col = [&](const QString &k) -> QString {
        if (!o.contains(k))
            return QString();
        QVariant v = o.value(k).toVariant();
        if (v.typeId() == QMetaType::QString) {
            QString s = v.toString();
            if (s.startsWith("#") && s.length() >= 7)
                return s.left(7);
            return s;
        }
        if (v.canConvert<QVariantList>()) {
            QVariantList l = v.toList();
            if (l.size() >= 3)
                return QString("#%1%2%3")
                    .arg(l[0].toInt(), 2, 16, QChar('0'))
                    .arg(l[1].toInt(), 2, 16, QChar('0'))
                    .arg(l[2].toInt(), 2, 16, QChar('0'));
        }
        return QString();
    };

    QString bg = col("background");
    QString fg = col("foreground");
    QString accent = col("color4"); // use a pleasant color4 as accent
    if (accent.isEmpty()) accent = col("color1");
    QString text2 = col("color8");

    QMap<QString, QString> updates;
    if (!bg.isEmpty()) updates[KEY_BG] = bg;
    if (!fg.isEmpty()) updates[KEY_TEXT1] = fg;
    if (!accent.isEmpty()) updates[KEY_ACCENT] = accent;
    if (!accent.isEmpty()) updates[KEY_BORDER] = accent;
    if (!text2.isEmpty()) updates[KEY_TEXT2] = text2;

    if (updates.isEmpty())
        return false;

    for (auto it = updates.begin(); it != updates.end(); ++it)
        m_theme[it.key()] = it.value();
    writeToDisk();
    emit themeChanged();
    return true;
}

void ThemeManager::reload()
{
    loadFromDisk();
}
