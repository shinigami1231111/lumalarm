#ifndef SOUNDRESOLVER_H
#define SOUNDRESOLVER_H

#include <QString>
#include <QStringList>

// Resolution chain for the sound an alarm rings with (guide):
//   per-alarm sound -> global default sound -> first tone found -> empty.
// Pure path logic - no QObject, no settings access: the caller (the Phase 4
// controller) supplies the two configured names and interprets an empty
// result as "ring without sound" (the window still rings, no audio).
//
// Search semantics are TonePaths' own, so a file the tone list shows is
// always a file this resolves: a string with a path separator is taken as
// given (must exist as a file), a bare name is searched in the tone
// directories user folder first, then the system folders.
class SoundResolver
{
public:
    SoundResolver() = default;
    // Test seam: fixed search dirs. Empty (the default) uses the real
    // TonePaths locations.
    explicit SoundResolver(const QStringList &searchDirs);

    QString resolve(const QString &perAlarmSound,
                    const QString &globalDefaultSound) const;

private:
    QString lookup(const QString &nameOrPath) const;
    QStringList searchDirs() const;

    QStringList m_searchDirs;
};

#endif // SOUNDRESOLVER_H
