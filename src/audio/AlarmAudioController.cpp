#include "audio/AlarmAudioController.h"
#include "audio/MprisPause.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include <cmath>
#include <utility>

namespace {
// A 1.5 s, 8 kHz, mono 16-bit PCM chime. Generated locally so a brand-new
// profile can preview a sound before any tone file has been imported; no
// external command and nothing written into the user's tones folder.
constexpr int kFallbackSampleRate = 8000;
constexpr int kFallbackFrames = 12000;

bool writeFallbackTone(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const int dataBytes = kFallbackFrames * 2;
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataBytes);
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(kFallbackSampleRate)
        << quint32(kFallbackSampleRate * 2) << quint16(2) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(dataBytes);

    constexpr double Pi = 3.14159265358979323846;
    for (int i = 0; i < kFallbackFrames; ++i) {
        const double t = static_cast<double>(i) / kFallbackSampleRate;
        const double freq = t < 0.5 ? 880.0 : (t < 1.0 ? 660.0 : 880.0);
        const double envelope = qMin(1.0, qMin(t * 20.0, (1.5 - t) * 20.0));
        const double sample = 12000.0 * envelope * std::sin(2.0 * Pi * freq * t);
        out << static_cast<qint16>(sample);
    }
    return out.status() == QDataStream::Ok && file.flush();
}
} // namespace

AlarmAudioController::AlarmAudioController(QObject *parent)
    : QObject(parent)
    , m_mixer(SystemMixer::create(this))
    , m_player(new AlarmPlayer(this))
    , m_journal(new RestoreJournal(QString(), this))
    , m_mpris(new MprisPause(this))
{
    m_ownsMixer = m_ownsPlayer = m_ownsJournal = true;
    m_backendCandidates = {QStringLiteral("pactl"), QStringLiteral("wpctl"),
                           QStringLiteral("amixer"),
                           QStringLiteral("null")};

    m_player->setMixer(m_mixer);
    connect(m_player, &AlarmPlayer::playbackError, this,
            &AlarmAudioController::playbackError);
    connect(m_player, &AlarmPlayer::isPlayingChanged, this,
            &AlarmAudioController::isPlayingChanged);
    connectMixer(m_mixer);
}

AlarmAudioController::AlarmAudioController(SystemMixer *mixer,
                                           AlarmPlayer *player,
                                           RestoreJournal *journal,
                                           const SoundResolver &resolver,
                                           MprisPause *mpris, QObject *parent)
    : QObject(parent)
    , m_mixer(mixer)
    , m_player(player)
    , m_journal(journal)
    , m_mpris(mpris)
    , m_resolver(resolver)
{
    if (m_mixer)
        m_player->setMixer(m_mixer);
    connect(m_player, &AlarmPlayer::playbackError, this,
            &AlarmAudioController::playbackError);
    connect(m_player, &AlarmPlayer::isPlayingChanged, this,
            &AlarmAudioController::isPlayingChanged);
    if (m_mixer)
        connectMixer(m_mixer);
}

AlarmAudioController::~AlarmAudioController()
{
    // On the normal exit path quit() already restored; anything still
    // managed here is crash-like: leave the journal so the next start
    // recovers (PipeWire keeps the sink state anyway).
    if (m_player)
        m_player->stop();
}

void AlarmAudioController::connectMixer(SystemMixer *mixer)
{
    connect(mixer, &SystemMixer::backendDetected, this,
            &AlarmAudioController::onBackendDetected);
    connect(mixer, &SystemMixer::sinkReady, this,
            &AlarmAudioController::onRingSinkReady);
    connect(mixer, &SystemMixer::sinkChanged, this,
            &AlarmAudioController::onSinkChanged);
    connect(mixer, &SystemMixer::volumeApplied, this, [this](const QString &) {
        if (m_restoring)
            onRestoreVolumeApplied();
    });
    connect(mixer, &SystemMixer::muteApplied, this, [this](const QString &) {
        if (m_restoring)
            onRestoreMuteApplied();
    });
    connect(mixer, &SystemMixer::volumeError, this,
            [this](const QString &reason) {
                if (m_restoring)
                    onRestoreError(QStringLiteral("volume restore failed: ")
                                   + reason);
            });
    connect(mixer, &SystemMixer::muteError, this,
            [this](const QString &reason) {
                if (m_restoring)
                    onRestoreError(QStringLiteral("mute restore failed: ")
                                   + reason);
            });
    // Speaker selector: the only consumer of the device list is this
    // controller (QML reads outputDevices through the NOTIFY signal).
    connect(mixer, &SystemMixer::sinksReady, this,
            &AlarmAudioController::onSinksReady);
    connect(mixer, &SystemMixer::sinksError, this, [](const QString &reason) {
        qWarning() << "audio device list unavailable:" << reason;
    });
}

void AlarmAudioController::start()
{
    if (!m_mixer)
        return;
    m_mixer->detect(); // -> onBackendDetected(); probe + crash recovery
}

void AlarmAudioController::applySettings(const Settings &settings)
{
    const bool sinkChanged = m_settings.outputSink != settings.outputSink;
    m_settings = settings;
    m_player->setStreamVolume(m_settings.streamPercent);
    m_player->setOutputSink(m_settings.outputSink);
    // Warm the backend's name -> id cache (wpctl resolves ids lazily) only
    // when the device itself changed, so unrelated configChanged() writes
    // never spawn a CLI process.
    if (sinkChanged && !m_settings.outputSink.isEmpty() && m_mixer)
        m_mixer->listSinks();
}

void AlarmAudioController::refreshOutputDevices()
{
    if (m_mixer)
        m_mixer->listSinks();
}

QVariantList AlarmAudioController::outputDevices() const
{
    return m_outputDevices;
}

void AlarmAudioController::onSinksReady(const QList<SinkState> &sinks)
{
    QVariantList devices;
    for (const SinkState &sink : sinks) {
        if (sink.name.isEmpty())
            continue; // cannot be addressed for routing (or persisted)
        QVariantMap device;
        device.insert(QStringLiteral("name"), sink.name);
        device.insert(QStringLiteral("description"),
                      sink.description.isEmpty() ? sink.name
                                                 : sink.description);
        devices.append(device);
    }
    m_outputDevices = devices;
    emit outputDevicesChanged();
}

bool AlarmAudioController::isPlaying() const
{
    return m_player->isPlaying();
}

// ── QML surface (old AudioPlayer API) ────────────────────────────────────────

void AlarmAudioController::play(const QString &toneName)
{
    const QString resolved =
        m_resolver.resolve(toneName, m_settings.defaultSound);
    if (resolved.isEmpty()) {
        emit playbackError(QStringLiteral("Sound file not found: ")
                           + toneName);
        return;
    }
    m_player->setStreamVolume(m_settings.streamPercent);
    m_player->play(resolved);
}

void AlarmAudioController::preview(const QString &toneName)
{
    const QString resolved =
        m_resolver.resolve(toneName, m_settings.defaultSound);
    if (resolved.isEmpty()) {
        emit playbackError(QStringLiteral("Sound file not found: ")
                           + toneName);
        return;
    }
    m_player->setStreamVolume(m_settings.streamPercent);
    m_player->preview(resolved);
}

QString AlarmAudioController::ensureFallbackTonePath()
{
    if (!m_fallbackToneDir) {
        m_fallbackToneDir.reset(new QTemporaryDir(
            QDir::tempPath() + QStringLiteral("/lumalarm-preview-XXXXXX")));
        if (!m_fallbackToneDir->isValid()) {
            emit playbackError(QStringLiteral("Could not create a temporary preview tone"));
            return {};
        }
    }

    QString path = m_fallbackToneDir->filePath(QStringLiteral("alarm.wav"));
    if (!QFile::exists(path) && !writeFallbackTone(path)) {
        emit playbackError(QStringLiteral("Could not generate a preview tone"));
        return {};
    }
    return path;
}

void AlarmAudioController::previewDefaultTone()
{
    const QString path = ensureFallbackTonePath();
    if (!path.isEmpty())
        preview(path);
}

void AlarmAudioController::previewWithAlarmVolume(const QString &toneName)
{
    const QString resolved = m_resolver.resolve(toneName, m_settings.defaultSound);
    if (resolved.isEmpty()) {
        emit playbackError(QStringLiteral("Sound file not found: ") + toneName);
        return;
    }
    ring(resolved);
}

void AlarmAudioController::previewDefaultToneWithAlarmVolume()
{
    const QString path = ensureFallbackTonePath();
    if (!path.isEmpty())
        ring(path);
}

void AlarmAudioController::stop()
{
    // Stopping during a ring is a full ring stop: the sink must come back.
    if (m_ringing) {
        stopRing();
        return;
    }
    m_player->stop();
}

void AlarmAudioController::reinitializeAudio()
{
    m_player->reinitializeAudio();
}

// ── the ring ─────────────────────────────────────────────────────────────────

void AlarmAudioController::ring(const QString &perAlarmSound)
{
    // Only one alarm rings at a time: a new ring fully stops the previous
    // one, restoring its sink first (old behaviour + policy restore).
    if (m_ringing)
        stopRing();

    m_ringing = true;
    m_mpris->pauseAll();

    const QString resolved =
        m_resolver.resolve(perAlarmSound, m_settings.defaultSound);
    if (resolved.isEmpty()) {
        qInfo() << "alarm rings without sound (no tone resolved)";
        return; // the window still rings; nothing to raise or restore
    }

    m_player->setStreamVolume(m_settings.streamPercent);
    m_player->play(resolved); // stream verification runs inside

    if (m_settings.policy != VolumePolicy::Kind::Guarantee)
        return; // alarmOnly: never touches the sink
    if (m_recoveryBusy || m_restoring) {
        // Startup recovery still owns the journal/sink: play, don't manage.
        qInfo() << "sink management skipped: restore still in flight";
        return;
    }

    m_awaitingRingSink = true;
    m_mixer->getDefaultSink();
}

void AlarmAudioController::onRingSinkReady(const SinkState &sink)
{
    if (!m_awaitingRingSink)
        return; // a stale or foreign answer (recovery, another requester)
    m_awaitingRingSink = false;
    if (!m_ringing || m_overridden)
        return;

    const auto decision = VolumePolicy::decideRaise(
        m_settings.policy, sink.volumes, sink.mute, m_settings.floorPercent);
    if (!decision.touchSink)
        return; // already loud enough: the user owns their volume

    // Crash-safety contract: the journal is written BEFORE the sink is
    // touched, and a failed write means the sink is never touched.
    RestoreJournal::Entry entry;
    entry.sinkName = sink.name;
    entry.originalVolumes = sink.volumes;
    entry.originalMute = sink.mute;
    entry.appliedVolumes = decision.targetVolumes;
    entry.appliedMute = false; // guarantee unmutes
    if (!m_journal->record(entry)) {
        qWarning() << "restore journal not written; sink untouched";
        return;
    }

    m_managedSink = sink;
    m_appliedVolumes = decision.targetVolumes;
    m_appliedMute = false;
    m_managing = true;
    m_overridden = false;
    m_mixer->setSinkVolumes(sink.name, decision.targetVolumes);
    if (decision.unmute)
        m_mixer->setSinkMute(sink.name, false);
    m_mixer->startWatch();
}

void AlarmAudioController::onSinkChanged(const SinkState &sink)
{
    if (!m_managing || m_overridden || sink.name != m_managedSink.name)
        return;
    if (!VolumePolicy::isUserOverride(m_appliedVolumes, m_appliedMute,
                                      sink.volumes, sink.mute))
        return;
    // The user moved the sink beyond the 3 % tolerance mid-ring: stop
    // managing, drop the journal, never restore, never fight (guide).
    qInfo() << "user changed the alarm sink; Lumalarm stops managing it";
    m_overridden = true;
    m_journal->clear();
    m_mixer->stopWatch();
}

void AlarmAudioController::stopRing()
{
    m_ringing = false;
    m_awaitingRingSink = false;
    m_player->stop();
    m_mpris->resumeAll();
    startRestore();
}

void AlarmAudioController::startRestore()
{
    if (!m_managing || m_restoring)
        return;
    if (m_overridden) {
        // The journal was already dropped when the override was detected.
        m_managing = false;
        m_mixer->stopWatch();
        return;
    }
    m_restoring = true;
    m_mixer->stopWatch();
    m_mixer->setSinkVolumes(m_managedSink.name, m_managedSink.volumes);
}

void AlarmAudioController::onRestoreVolumeApplied()
{
    m_mixer->setSinkMute(m_managedSink.name, m_managedSink.mute);
}

void AlarmAudioController::onRestoreMuteApplied()
{
    m_journal->clear();
    m_managing = false;
    m_restoring = false;
    qInfo() << "alarm sink restored exactly";
    emit restoreSettled(true);
}

void AlarmAudioController::onRestoreError(const QString &what)
{
    // Keep the journal: next start's recover() retries (crash-safety).
    qWarning() << what << "- journal kept for the next start";
    m_managing = false;
    m_restoring = false;
    emit restoreSettled(false);
}

void AlarmAudioController::shutdownForQuit()
{
    m_ringing = false;
    m_awaitingRingSink = false;
    m_player->stop();
    m_mpris->resumeAll();

    if (!m_managing) {
        if (m_overridden)
            m_journal->clear();
        return;
    }
    if (m_overridden) {
        m_journal->clear();
        m_managing = false;
        return;
    }
    if (m_restoring)
        return; // already in flight; the bounded wait below observes it

    m_restoring = true;
    m_mixer->stopWatch();
    m_mixer->setSinkVolumes(m_managedSink.name, m_managedSink.volumes);
    if (!m_restoring)
        return; // completed synchronously (restoreSettled already emitted)

    // Bounded synchronous wait: the guide demands the restore before
    // teardown but never stalls session end. On timeout the journal
    // survives and recover() at next start finishes the job.
    QEventLoop loop;
    connect(this, &AlarmAudioController::restoreSettled, &loop,
            &QEventLoop::quit);
    QTimer::singleShot(800, &loop, &QEventLoop::quit);
    loop.exec();
}

// ── backend selection + crash recovery ───────────────────────────────────────

void AlarmAudioController::onBackendDetected(const QString &backendName,
                                             const MixerCapabilities &capabilities)
{
    m_currentBackendName = backendName;
    if (m_backendResolved)
        return;
    if (!capabilities.canSetVolume) {
        tryNextBackend();
        return;
    }
    m_backendResolved = true;

    // Crash recovery: undo what a previous run left behind, before any new
    // management may start (the journal refuses record() while recovering).
    m_recoveryBusy = true;
    connect(m_journal, &RestoreJournal::recoveryFinished, this,
            [this](bool, const QString &reason) {
                m_recoveryBusy = false;
                qInfo() << "startup audio recovery:" << reason;
            },
            Qt::SingleShotConnection);
    m_journal->recover(m_mixer);
}

void AlarmAudioController::tryNextBackend()
{
    while (!m_backendCandidates.isEmpty()) {
        const QString next = m_backendCandidates.takeFirst();
        if (next == m_currentBackendName)
            continue; // already failed
        SystemMixer *replacement =
            SystemMixer::createBackend(next, this);
        if (!replacement)
            continue;
        qInfo() << "audio backend" << m_currentBackendName
                << "unusable, falling back to" << next;
        if (m_ownsMixer)
            m_mixer->deleteLater();
        else if (m_mixer)
            m_mixer->disconnect(this);
        m_mixer = replacement;
        m_ownsMixer = true;
        m_player->setMixer(m_mixer);
        connectMixer(m_mixer);
        m_mixer->detect();
        return;
    }
    // No usable backend: sink management stays off; playback (QAudioOutput)
    // and stream verification degrade gracefully.
    qWarning() << "no usable audio backend; sink management disabled";
    m_backendResolved = true;
}
