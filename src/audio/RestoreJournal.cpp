#include "audio/RestoreJournal.h"
#include "audio/VolumePolicy.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject entryToJson(const RestoreJournal::Entry &entry)
{
    QJsonArray originalVolumes;
    for (int volume : entry.originalVolumes)
        originalVolumes.append(volume);
    QJsonArray appliedVolumes;
    for (int volume : entry.appliedVolumes)
        appliedVolumes.append(volume);

    QJsonObject original;
    original.insert(QStringLiteral("volumes"), originalVolumes);
    original.insert(QStringLiteral("mute"), entry.originalMute);
    QJsonObject applied;
    applied.insert(QStringLiteral("volumes"), appliedVolumes);
    applied.insert(QStringLiteral("mute"), entry.appliedMute);

    QJsonObject root;
    root.insert(QStringLiteral("version"), entry.version);
    root.insert(QStringLiteral("sinkName"), entry.sinkName);
    root.insert(QStringLiteral("original"), original);
    root.insert(QStringLiteral("applied"), applied);
    root.insert(QStringLiteral("writtenAtMs"),
                static_cast<double>(entry.writtenAtMs));
    return root;
}

bool readVolumes(const QJsonObject &object, const QString &key,
                 QVector<int> &volumes)
{
    const QJsonValue value = object.value(key);
    if (!value.isArray())
        return false;
    const QJsonArray array = value.toArray();
    if (array.isEmpty())
        return false;
    for (const auto &item : array) {
        if (!item.isDouble())
            return false;
        volumes.append(item.toInt());
    }
    return true;
}

std::optional<RestoreJournal::Entry> entryFromJson(const QJsonObject &root)
{
    if (root.value(QStringLiteral("version")).toInt(-1)
        != RestoreJournal::kCurrentVersion)
        return std::nullopt; // unsupported version: caller deletes it

    RestoreJournal::Entry entry;
    entry.version = RestoreJournal::kCurrentVersion;
    entry.sinkName = root.value(QStringLiteral("sinkName")).toString();
    if (entry.sinkName.isEmpty())
        return std::nullopt;

    const QJsonObject original =
        root.value(QStringLiteral("original")).toObject();
    const QJsonObject applied = root.value(QStringLiteral("applied")).toObject();
    if (!readVolumes(original, QStringLiteral("volumes"),
                     entry.originalVolumes))
        return std::nullopt;
    if (!readVolumes(applied, QStringLiteral("volumes"), entry.appliedVolumes))
        return std::nullopt;
    entry.originalMute = original.value(QStringLiteral("mute")).toBool(false);
    entry.appliedMute = applied.value(QStringLiteral("mute")).toBool(false);
    entry.writtenAtMs = static_cast<qint64>(
        root.value(QStringLiteral("writtenAtMs")).toDouble(0));
    return entry;
}

} // namespace

QString RestoreJournal::defaultPath()
{
    QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (stateHome.isEmpty())
        stateHome = QDir::homePath() + QStringLiteral("/.local/state");
    while (stateHome.endsWith(QLatin1Char('/')))
        stateHome.chop(1);
    return stateHome + QStringLiteral("/lumalarm/audio-restore.json");
}

RestoreJournal::RestoreJournal(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path.isEmpty() ? defaultPath() : path)
{
}

bool RestoreJournal::record(const Entry &entry)
{
    if (m_phase != Phase::Idle) {
        qWarning() << "restore journal is busy; refusing to record";
        return false;
    }
    if (entry.sinkName.isEmpty() || entry.originalVolumes.isEmpty()
        || entry.appliedVolumes.isEmpty()) {
        qWarning() << "refusing to record an incomplete restore journal";
        return false;
    }

    Entry toWrite = entry;
    toWrite.version = kCurrentVersion;
    if (toWrite.writtenAtMs <= 0)
        toWrite.writtenAtMs = QDateTime::currentMSecsSinceEpoch();

    // Fresh machines have never created the app's state directory, and
    // QSaveFile cannot make it: without this the write-ahead always fails,
    // the controller (correctly) never touches the sink, and the floor
    // silently does nothing on every new install (Phase 6 field report).
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        qWarning() << "restore journal directory unavailable:"
                   << QFileInfo(m_path).absolutePath()
                   << "- refusing to touch the sink";
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "restore journal not written:" << file.errorString()
                   << "- refusing to touch the sink";
        return false;
    }
    // Guide: journal is private to the user (0600).
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(
        QJsonDocument(entryToJson(toWrite)).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qWarning() << "restore journal commit failed:" << file.errorString()
                   << "- refusing to touch the sink";
        return false;
    }
    return true;
}

std::optional<RestoreJournal::Entry> RestoreJournal::load()
{
    if (!QFile::exists(m_path))
        return std::nullopt; // nothing pending: the normal case

    QFile file(m_path);
    QByteArray raw;
    const bool opened = file.open(QIODevice::ReadOnly);
    if (opened)
        raw = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
    std::optional<Entry> entry;
    if (opened && parseError.error == QJsonParseError::NoError
        && document.isObject())
        entry = entryFromJson(document.object());

    if (!entry) {
        qWarning() << "corrupt restore journal at" << m_path
                   << "- removing it";
        if (opened)
            QFile::remove(m_path); // never crash on a damaged journal
        return std::nullopt;
    }
    return entry;
}

bool RestoreJournal::clear()
{
    if (!QFile::exists(m_path))
        return true;
    if (!QFile::remove(m_path)) {
        qWarning() << "could not remove restore journal" << m_path;
        return false;
    }
    return true;
}

void RestoreJournal::recover(SystemMixer *mixer)
{
    if (m_phase != Phase::Idle) {
        emit recoveryFinished(false,
                              QStringLiteral("recovery already in progress"));
        return;
    }
    if (!mixer) {
        emit recoveryFinished(false, QStringLiteral("no mixer available"));
        return;
    }

    const auto entry = load();
    if (!entry) {
        emit recoveryFinished(false, QStringLiteral("no journal pending"));
        return;
    }

    m_entry = *entry;
    m_mixer = mixer;
    m_phase = Phase::WaitingSink;

    m_connections
        << connect(mixer, &SystemMixer::sinkReady, this,
                   [this](const SinkState &sink) { handleSinkRead(sink); })
        << connect(mixer, &SystemMixer::sinkError, this,
                   [this](const QString &reason) {
                       if (m_phase != Phase::WaitingSink)
                           return;
                       // Keep the journal: the sink may come back (replug,
                       // busy audio server) and the next start retries.
                       finish(false,
                              QStringLiteral("cannot read sink to verify: ")
                                  + reason);
                   });
    mixer->getSink(m_entry.sinkName);
}

void RestoreJournal::handleSinkRead(const SinkState &sink)
{
    if (m_phase != Phase::WaitingSink)
        return;

    if (VolumePolicy::isUserOverride(m_entry.appliedVolumes,
                                     m_entry.appliedMute, sink.volumes,
                                     sink.mute)) {
        // The sink no longer reflects what we applied: either the user
        // changed it (no restore - guide) or it was never applied before
        // the crash (nothing to undo). Either way, leave it alone.
        clear();
        finish(false,
               QStringLiteral("sink differs from the journal; left untouched"));
        return;
    }

    // Compare-and-set succeeded: write the original state back exactly.
    m_phase = Phase::SettingVolume;
    m_connections
        << connect(m_mixer, &SystemMixer::volumeApplied, this,
                   [this](const QString &) {
                       if (m_phase != Phase::SettingVolume)
                           return;
                       m_phase = Phase::SettingMute;
                       m_connections
                           << connect(m_mixer, &SystemMixer::muteApplied, this,
                                      [this](const QString &) {
                                          if (m_phase != Phase::SettingMute)
                                              return;
                                          clear();
                                          finish(true,
                                                 QStringLiteral(
                                                     "sink restored"));
                                      })
                           << connect(m_mixer, &SystemMixer::muteError, this,
                                      [this](const QString &reason) {
                                          if (m_phase != Phase::SettingMute)
                                              return;
                                          finish(false,
                                                 QStringLiteral(
                                                     "mute restore failed: ")
                                                     + reason);
                                      });
                       m_mixer->setSinkMute(m_entry.sinkName,
                                            m_entry.originalMute);
                   })
        << connect(m_mixer, &SystemMixer::volumeError, this,
                   [this](const QString &reason) {
                       if (m_phase != Phase::SettingVolume)
                           return;
                       finish(false,
                              QStringLiteral("volume restore failed: ")
                                  + reason); // journal kept for a retry
                   });
    m_mixer->setSinkVolumes(m_entry.sinkName, m_entry.originalVolumes);
}

void RestoreJournal::finish(bool restored, const QString &reason)
{
    for (const QMetaObject::Connection &connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    m_phase = Phase::Idle;
    m_entry = Entry{};
    m_mixer = nullptr;
    emit recoveryFinished(restored, reason);
}
