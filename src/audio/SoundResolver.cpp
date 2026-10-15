#include "audio/SoundResolver.h"
#include "managers/TonePaths.h"

#include <QDir>
#include <QFileInfo>

SoundResolver::SoundResolver(const QStringList &searchDirs)
    : m_searchDirs(searchDirs)
{
}

QStringList SoundResolver::searchDirs() const
{
    return m_searchDirs.isEmpty() ? TonePaths::searchDirectories()
                                  : m_searchDirs;
}

QString SoundResolver::lookup(const QString &nameOrPath) const
{
    if (nameOrPath.isEmpty())
        return {};

    // Absolute (or relative) paths keep their old meaning: valid file as-is.
    if (nameOrPath.contains(QLatin1Char('/'))
        || nameOrPath.contains(QLatin1Char('\\'))) {
        const QFileInfo info(nameOrPath);
        return info.isFile() ? nameOrPath : QString();
    }

    const QStringList dirs = searchDirs();
    for (const QString &dir : dirs) {
        const QString candidate = dir + QLatin1Char('/') + nameOrPath;
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    return {};
}

QString SoundResolver::resolve(const QString &perAlarmSound,
                               const QString &globalDefaultSound) const
{
    // 1. the alarm's own sound
    QString own = lookup(perAlarmSound);
    if (!own.isEmpty())
        return own;

    // 2. the configured global default
    QString fallback = lookup(globalDefaultSound);
    if (!fallback.isEmpty())
        return fallback;

    // 3. first tone in the search dirs (user folder first, name order) so a
    //    ring never starts silent while ANY tone exists.
    const QStringList dirs = searchDirs();
    for (const QString &dir : dirs) {
        const QFileInfoList tones =
            QDir(dir).entryInfoList(TonePaths::nameFilters(),
                                     QDir::Files | QDir::Readable,
                                     QDir::Name);
        if (!tones.isEmpty())
            return tones.first().absoluteFilePath();
    }

    // 4. nothing anywhere: the caller rings without sound.
    return {};
}
