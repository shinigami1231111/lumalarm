#ifndef TONEPATHS_H
#define TONEPATHS_H

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

// Single source of truth for where tones live (Issue #2).
//
// ConfigManager (discovery, import, delete) and SoundResolver/AlarmPlayer
// (playback) both go through this header, so a file the tone list shows is
// always a file the player can open. Discovery and the import dialog must
// likewise agree on what counts as a tone — supportedSuffixes() drives both.
//
// Two locations, one policy:
//   * user   — <config>/lumalarm/tones. The only writable folder: imports land
//              here and deleteTone() only ever touches it.
//   * system — <data>/lumalarm/tones for every XDG data location (by default
//              /usr/local/share and /usr/share). The Arch package creates
//              /usr/share/lumalarm/tones; these are read-only drop-ins.
//
// Header-only on purpose: it needs no QObject, so every test binary that
// already compiles SoundResolver.cpp or ConfigManager.cpp links it unchanged.
namespace TonePaths {

// The folder the user imports into and deletes from.
inline QString userDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           QStringLiteral("/lumalarm/tones");
}

// Package-installed, read-only tone folders that currently exist. The
// user-writable $XDG_DATA_HOME is skipped: it is not a system location and the
// config folder above is the one this app documents for user files.
inline QStringList systemDirectories()
{
    QStringList out;
    const QString userHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList bases =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &base : bases) {
        if (base == userHome)
            continue;
        const QString dir = base + QStringLiteral("/lumalarm/tones");
        if (QDir(dir).exists())
            out.append(dir);
    }
    return out;
}

// Ordered lookup: the user folder first so a user file of the same name wins
// over a packaged one, then the system folders.
inline QStringList searchDirectories()
{
    QStringList out;
    out.append(userDirectory());
    const QStringList system = systemDirectories();
    for (const QString &dir : system)
        out.append(dir);
    return out;
}

// What counts as a tone. The import dialog's nameFilters in SoundManager.qml
// must list exactly these extensions, otherwise an import can succeed and then
// silently vanish from the list (the reported .aac case).
inline QStringList supportedSuffixes()
{
    return {QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg"),
            QStringLiteral("flac"), QStringLiteral("aac")};
}

inline bool isSupportedToneFile(const QString &fileName)
{
    return supportedSuffixes().contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive);
}

// QDir name filters built from supportedSuffixes(), so discovery can never
// drift from the format list.
inline QStringList nameFilters()
{
    QStringList filters;
    const QStringList suffixes = supportedSuffixes();
    filters.reserve(suffixes.size());
    for (const QString &suffix : suffixes)
        filters.append(QStringLiteral("*.") + suffix);
    return filters;
}

// Resolves what the UI hands over (a bare tone file name) to a real file.
// Paths containing a separator are taken as given, exactly like the pre-Issue-#2
// behavior of AudioPlayer::resolvePath(); bare names are searched user folder
// first, then the system folders. Returns an empty string when nothing matches.
inline QString resolve(const QString &filePath)
{
    if (filePath.isEmpty())
        return {};

    if (filePath.contains(QLatin1Char('/')) || filePath.contains(QLatin1Char('\\'))) {
        const QFileInfo fi(filePath);
        return fi.isFile() ? filePath : QString();
    }

    const QStringList dirs = searchDirectories();
    for (const QString &dir : dirs) {
        const QString candidate = dir + QLatin1Char('/') + filePath;
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    return {};
}

} // namespace TonePaths

#endif // TONEPATHS_H
