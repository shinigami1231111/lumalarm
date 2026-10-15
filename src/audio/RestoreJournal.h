#ifndef RESTOREJOURNAL_H
#define RESTOREJOURNAL_H

#include "audio/SystemMixer.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <optional>

// Crash-safe write-ahead journal for the guarantee policy.
//
// PipeWire/PulseAudio persist sink volumes across crashes and reboots, so a
// ring that raised the sink MUST leave a record before touching it:
//   1. record() the original + the about-to-be-applied state FIRST —
//      a failed record() means callers must not touch the sink at all;
//   2. apply the raise;
//   3. on a clean stop, restore and clear();
//   4. after a crash, recover(mixer) at startup undoes the raise with
//      compare-and-set semantics: the original state is written back only
//      when the sink still matches what we applied (within tolerance —
//      otherwise the user has touched it and we leave it alone).
//
// The file is atomic (QSaveFile), mode 0600, versioned JSON. A corrupt or
// unsupported journal is logged and deleted (guide).
class RestoreJournal : public QObject
{
    Q_OBJECT

public:
    static constexpr int kCurrentVersion = 1;

    struct Entry {
        int version = kCurrentVersion;
        QString sinkName;
        QVector<int> originalVolumes; // raw per-channel, channel order kept
        bool originalMute = false;
        QVector<int> appliedVolumes;  // state we are about to write
        bool appliedMute = false;
        qint64 writtenAtMs = 0;
    };

    // $XDG_STATE_HOME, else ~/.local/state, + /lumalarm/audio-restore.json.
    static QString defaultPath();

    // Empty path => defaultPath(). The journal path is fixed for the
    // lifetime of the object (tests inject a QTemporaryDir path).
    explicit RestoreJournal(const QString &path = QString(),
                            QObject *parent = nullptr);

    QString path() const { return m_path; }

    // Write-ahead record. Returns false when the entry is incomplete, the
    // journal is busy, or the file cannot be written atomically — callers
    // MUST NOT apply sink changes in that case.
    bool record(const Entry &entry);

    // Loads the pending entry. A corrupt/unsupported/unreadable file is
    // logged and deleted, yielding std::nullopt (guide). A missing file is
    // the normal "nothing pending" case.
    std::optional<Entry> load();

    // Deletes the journal (after a successful restore or a deliberate skip).
    bool clear();

    // Async crash recovery; emits recoveryFinished(restored, reason).
    // Reads the journaled sink through the mixer, compares observed vs.
    // applied (VolumePolicy tolerance + mute), restores the original state
    // exactly when they match, then clears the journal. On sink read or
    // write errors the journal is KEPT so the next start retries.
    // The mixer is borrowed and must outlive the recovery.
    void recover(SystemMixer *mixer);

signals:
    void recoveryFinished(bool restored, const QString &reason);

private:
    enum class Phase { Idle, WaitingSink, SettingVolume, SettingMute };

    void handleSinkRead(const SinkState &sink);
    void finish(bool restored, const QString &reason);

    QString m_path;
    Entry m_entry;
    Phase m_phase = Phase::Idle;
    SystemMixer *m_mixer = nullptr; // borrowed, not owned
    QList<QMetaObject::Connection> m_connections;
};

#endif // RESTOREJOURNAL_H
