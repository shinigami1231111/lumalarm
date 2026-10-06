#include "managers/ConfigManager.h"
#include "managers/ThemeManager.h"
#include "managers/TonePaths.h"
#include <QFileInfo>
#include <QFileInfoList>

namespace {
// QML hands a url over as a JavaScript string in QUrl::PrettyDecoded form:
// spaces and non-ASCII are already decoded, while '%23' (#) and '%25' (%) stay
// percent-encoded. Hand-stripping "file://" — the pre-Issue-#2 code — therefore
// produced a path that did not exist for names containing '#' or '%'. Going
// through QUrl decodes what is encoded and tolerates what is not.
QString localPathFromUrlString(const QString &source)
{
    if (source.isEmpty())
        return {};

    const QUrl url(source);
    const QString scheme = url.scheme();
    if (scheme.isEmpty())
        return source; // plain filesystem path
    if (scheme == QLatin1String("file"))
        return url.toLocalFile();
    return {}; // http:/ftp:/… cannot be imported from a local picker
}
} // namespace

ConfigManager::ConfigManager(QObject *parent) : QObject(parent)
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
    m_firstRunProfile = !QFileInfo::exists(m_configDir + "/settings.ini") &&
                        !QFileInfo::exists(m_configDir + "/theme.conf");

    QDir().mkpath(m_configDir);

    m_tonesDir = TonePaths::userDirectory();
    QDir().mkpath(m_tonesDir);

    QString settingsPath = m_configDir + "/settings.ini";
    m_settings = new QSettings(settingsPath, QSettings::IniFormat, this);

    m_theme = new ThemeManager(this);
    connect(m_theme, &ThemeManager::themeChanged, this, &ConfigManager::configChanged);
}

void ConfigManager::migrateOldConfig()
{
    QString newDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
    QString oldDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/glass-alarm";

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

QString ConfigManager::systemTonesDirectory() const
{
    const QStringList system = TonePaths::systemDirectories();
    return system.isEmpty() ? QString() : system.first();
}

QStringList ConfigManager::availableTones() const
{
    QStringList tones;
    // searchDirectories() starts with the user folder, so a user file wins any
    // name clash with a packaged one; the list is sorted once at the end so the
    // sound combo boxes keep a single predictable order.
    const QStringList dirs = TonePaths::searchDirectories();
    for (const QString &dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList(
            TonePaths::nameFilters(), QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &fi : files) {
            if (!tones.contains(fi.fileName()))
                tones.append(fi.fileName());
        }
    }
    tones.sort();
    return tones;
}

QString ConfigManager::copyToTones(const QString &sourcePath)
{
    return importTone(sourcePath, false);
}

QString ConfigManager::replaceTone(const QString &sourcePath)
{
    return importTone(sourcePath, true);
}

QString ConfigManager::importTone(const QString &sourcePath, bool replaceExisting)
{
    setToneError(QString());

    const QString local = localPathFromUrlString(sourcePath);
    if (local.isEmpty()) {
        setToneError(QStringLiteral("invalid-path"));
        return {};
    }

    const QFileInfo fi(local);
    if (!fi.exists() || !fi.isFile()) {
        setToneError(QStringLiteral("source-missing"));
        return {};
    }

    // Checked before anything else, so an unsupported file can never be copied
    // into the tones folder and then sit there undiscovered.
    if (!TonePaths::isSupportedToneFile(fi.fileName())) {
        setToneError(QStringLiteral("unsupported"));
        return {};
    }

    QString fileName = fi.fileName(); // non-const: returned by value below

    // The picker offered a file that already lives in the tones folder:
    // nothing to copy, and it is already in the list.
    if (QDir::cleanPath(fi.absolutePath()) == QDir::cleanPath(m_tonesDir))
        return fileName;

    const QString destPath = m_tonesDir + QLatin1Char('/') + fileName;
    const bool destExists = QFileInfo::exists(destPath);

    // Issue #2's single duplicate policy is "ask": refuse here, let the UI
    // confirm, and come back through replaceTone().
    if (destExists && !replaceExisting) {
        setToneError(QStringLiteral("exists"), fileName);
        return {};
    }

    if (!QDir(m_tonesDir).exists() && !QDir().mkpath(m_tonesDir)) {
        setToneError(QStringLiteral("destination"));
        return {};
    }
    if (!QFileInfo(m_tonesDir).isWritable()) {
        setToneError(QStringLiteral("permission"));
        return {};
    }

    if (destExists) {
        // QFile::copy() never overwrites, so a confirmed replacement drops the
        // old file first.
        if (!QFile::remove(destPath)) {
            setToneError(QStringLiteral("permission"));
            return {};
        }
    }

    if (!QFile::copy(local, destPath)) {
        setToneError(QStringLiteral("copy-failed"));
        return {};
    }

    emit configChanged();
    return fileName;
}

QUrl ConfigManager::toneFileUrl(const QString &fileName) const
{
    const QString path = TonePaths::resolve(fileName);
    if (path.isEmpty())
        return {};
    return QUrl::fromLocalFile(path);
}

bool ConfigManager::deleteTone(const QString &fileName)
{
    setToneError(QString());

    if (fileName.isEmpty() || fileName.contains(QLatin1Char('/')) ||
        fileName.contains(QLatin1Char('\\'))) {
        setToneError(QStringLiteral("invalid-path"));
        return false;
    }

    const QString filePath = m_tonesDir + QLatin1Char('/') + fileName;
    if (!QFileInfo::exists(filePath)) {
        // A system tone shows up in the same list but lives in a read-only
        // folder — refusing must say so instead of failing silently.
        setToneError(TonePaths::resolve(fileName).isEmpty() ? QStringLiteral("not-found")
                                                            : QStringLiteral("read-only"),
                     fileName);
        return false;
    }

    if (!QFile::remove(filePath)) {
        setToneError(QStringLiteral("delete-failed"));
        return false;
    }

    emit configChanged();
    return true;
}

QString ConfigManager::toneError() const
{
    return m_toneError;
}

QString ConfigManager::toneErrorMessage() const
{
    // The codes are a stable contract with the QML and the unit tests; this is
    // the only place they become text.
    if (m_toneError.isEmpty())
        return {};

    if (m_toneError == QLatin1String("exists"))
        return tr("A tone named \"%1\" already exists.").arg(m_toneErrorDetail);
    if (m_toneError == QLatin1String("source-missing"))
        return tr("The selected file no longer exists.");
    if (m_toneError == QLatin1String("invalid-path"))
        return tr("The selected item is not a local file.");
    if (m_toneError == QLatin1String("unsupported"))
        return tr("Unsupported file. Use .wav, .mp3, .ogg, .flac or .aac.");
    if (m_toneError == QLatin1String("destination"))
        return tr("The tones folder could not be created.");
    if (m_toneError == QLatin1String("permission"))
        return tr("The tones folder is not writable.");
    if (m_toneError == QLatin1String("copy-failed"))
        return tr("The file could not be copied into the tones folder.");
    if (m_toneError == QLatin1String("not-found"))
        return tr("The tone \"%1\" no longer exists.").arg(m_toneErrorDetail);
    if (m_toneError == QLatin1String("read-only"))
        return tr("The tone \"%1\" is installed system-wide and cannot be deleted.")
            .arg(m_toneErrorDetail);
    if (m_toneError == QLatin1String("delete-failed"))
        return tr("The tone could not be deleted.");
    return tr("The operation failed.");
}

void ConfigManager::setToneError(const QString &code, const QString &detail)
{
    m_toneError = code;
    m_toneErrorDetail = detail;
    emit toneErrorChanged();
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
    return m_settings->value(QStringLiteral("trayIconEnabled"), m_theme->show_tray_icon()).toBool();
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
    m_settings->setValue(QStringLiteral("nightMode/minutesBefore"), qBound(1, minutes, 180));
    emit configChanged();
}

double ConfigManager::nightModeOpacity() const
{
    // Valid range 0.0..1.0; anything else in the file falls back to 0.4.
    const double opacity =
        m_settings->value(QStringLiteral("nightMode/dimOpacity"), 0.4).toDouble();
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
