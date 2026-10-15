#include "audio/WpctlMixer.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>

#include <memory>

WpctlMixer::WpctlMixer(QObject *parent)
    : SystemMixer(parent)
    , m_backend(new MixerBackend(this))
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() { fetchDefault(Purpose::Poll); });
}

WpctlMixer::~WpctlMixer()
{
    stopWatch();
}

void WpctlMixer::runWpctl(const QStringList &args, MixerBackend::Callback callback)
{
    m_backend->enqueue(QStringLiteral("wpctl"), args, std::move(callback));
}

int WpctlMixer::resolveId(const QString &nodeName) const
{
    if (nodeName.isEmpty())
        return -1;
    return m_nameToId.value(nodeName, -1);
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

void WpctlMixer::detect()
{
    runWpctl({QStringLiteral("status")},
             [this](const QByteArray &, const QByteArray &err, int code, bool timedOut) {
                 if (mixerCommandFailed(code, timedOut)) {
                     qWarning() << "Lumalarm: wpctl detect failed:"
                                << mixerFailureReason(err, code, timedOut);
                     emit backendDetected(QStringLiteral("wpctl"), MixerCapabilities{});
                     return;
                 }
                 MixerCapabilities caps;
                 caps.canSetVolume = true;
                 caps.canMute = true;
                 caps.perChannel = false; // wpctl exposes a single scalar
                 caps.hasSinkInputs = false;
                 caps.canSubscribe = false;
                 emit backendDetected(QStringLiteral("wpctl"), caps);
             });
}

void WpctlMixer::getDefaultSink()
{
    fetchDefault(Purpose::DefaultRequest);
}

void WpctlMixer::getSink(const QString &name)
{
    const int id = resolveId(name);
    if (id < 0) {
        // wpctl cannot look a sink up by node.name without an ID; the ID is
        // learned from getDefaultSink()/inspect, so an unvisited sink is not
        // reachable through this backend.
        emit sinkError(QStringLiteral("wpctl backend has not resolved sink id for ") + name);
        return;
    }
    fetchDefault(Purpose::NameLookup, id, name);
}

void WpctlMixer::listSinks()
{
    // status gives ids + display names only; node.name — the routing key —
    // needs one inspect per sink. Request-local state travels through
    // captures (guide); the FIFO keeps the ordering. Sinks whose node.name
    // cannot be learned are skipped: they cannot be addressed for routing.
    runWpctl({QStringLiteral("status")},
             // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
             [this](const QByteArray &out, const QByteArray &err, int code,
                    bool timedOut) {
                 if (mixerCommandFailed(code, timedOut)) {
                     emit sinksError(mixerFailureReason(err, code, timedOut));
                     return;
                 }
                 const QVector<WpSink> sinks = parseStatus(QString::fromUtf8(out));
                 if (sinks.isEmpty()) {
                     emit sinksReady({});
                     return;
                 }
                 auto remaining = std::make_shared<int>(sinks.size());
                 auto collected = std::make_shared<QList<SinkState>>();
                 for (const WpSink &sink : sinks) {
                     runWpctl(
                         {QStringLiteral("inspect"), QString::number(sink.id)},
                         [this, sink, remaining, collected](const QByteArray &inspectOut,
                                                            const QByteArray &, int inspectCode,
                                                            bool inspectTimedOut) {
                             if (inspectCode == 0 && !inspectTimedOut) {
                                 const QString nodeName = parseNodeName(
                                     QString::fromUtf8(inspectOut));
                                 if (!nodeName.isEmpty()) {
                                     SinkState state;
                                     state.index = sink.id;
                                     state.name = nodeName;
                                     state.description = sink.description;
                                     state.mute = sink.mute;
                                     m_idToNodeName.insert(sink.id, nodeName);
                                     m_nameToId.insert(nodeName, sink.id);
                                     m_idToDescription.insert(sink.id, sink.description);
                                     collected->append(state);
                                 }
                             }
                             if (--(*remaining) == 0)
                                 emit sinksReady(*collected);
                         });
                 }
             });
}

void WpctlMixer::moveSinkInput(int index, const QString &sinkName)
{
    if (index < 0 || sinkName.isEmpty()) {
        emit moveError(QStringLiteral("invalid stream or sink"));
        return;
    }
    const int target = resolveId(sinkName);
    if (target < 0) {
        // Cold cache (listSinks not run yet): the controller warms it at
        // startup whenever an output device is configured.
        emit moveError(QStringLiteral("wpctl has not resolved sink id for ") + sinkName);
        return;
    }
    runWpctl({QStringLiteral("move"), QString::number(index),
              QString::number(target)},
             [this, index](const QByteArray &, const QByteArray &err, int code,
                           bool timedOut) {
                 if (!mixerCommandFailed(code, timedOut))
                     emit moveApplied(index);
                 else
                     emit moveError(mixerFailureReason(err, code, timedOut));
             });
}

void WpctlMixer::fetchDefault(Purpose purpose, int id, const QString &name)
{
    if (id >= 0) {
        // Already known: skip status, go straight to get-volume.
        runWpctl({QStringLiteral("get-volume"), QString::number(id)},
                 [this, purpose, id, name](const QByteArray &out, const QByteArray &err,
                                           int code, bool timedOut) {
                     handleGetVolume(purpose, id, name, QString(), 1.0f, false, out, err, code,
                                     timedOut);
                 });
        return;
    }

    runWpctl({QStringLiteral("status")},
             [this, purpose, name](const QByteArray &out, const QByteArray &err, int code,
                                   bool timedOut) {
                 handleStatus(purpose, name, out, err, code, timedOut);
             });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void WpctlMixer::handleStatus(Purpose purpose, const QString &wantedName, const QByteArray &out,
                              const QByteArray &err, int exitCode, bool timedOut)
{
    if (mixerCommandFailed(exitCode, timedOut)) {
        if (purpose == Purpose::DefaultRequest)
            emit sinkError(mixerFailureReason(err, exitCode, timedOut));
        else if (purpose == Purpose::Poll)
            qWarning() << "Lumalarm: wpctl status refresh failed:"
                       << mixerFailureReason(err, exitCode, timedOut);
        return; // NameLookup: silence (getSink already resolved the id)
    }

    const QVector<WpSink> sinks = parseStatus(QString::fromUtf8(out));
    const WpSink *target = nullptr;
    for (const WpSink &sink : sinks) {
        if (sink.isDefault) {
            target = &sink;
            break;
        }
    }
    if (!target && !sinks.isEmpty())
        target = &sinks.first();

    if (!target) {
        if (purpose == Purpose::DefaultRequest)
            emit sinkError(QStringLiteral("wpctl reports no sinks"));
        return;
    }

    if (purpose == Purpose::Poll && target->id == m_defaultSinkId
        && qFuzzyCompare(target->volume + 1.0f, m_defaultVolume + 1.0f)
        && target->mute == m_defaultMute) {
        return; // unchanged: stay quiet
    }

    const int id = target->id;
    const QString description = target->description;
    const float statusVolume = target->volume;
    const bool statusMute = target->mute;

    runWpctl({QStringLiteral("get-volume"), QString::number(id)},
             [this, purpose, id, wantedName, description, statusVolume,
              statusMute](const QByteArray &out2, const QByteArray &err2, int code2,
                          bool to2) {
                 handleGetVolume(purpose, id, wantedName, description, statusVolume,
                                 statusMute, out2, err2, code2, to2);
             });
}

void WpctlMixer::handleGetVolume(Purpose purpose, int id, const QString &name,
                                 const QString &description, float statusVolume, bool statusMute,
                                 // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                                 const QByteArray &out, const QByteArray &err, int exitCode,
                                 bool timedOut)
{
    // Degrade gracefully: if get-volume fails we keep the (coarser) status
    // values instead of aborting the chain.
    float volume = statusVolume;
    bool mute = statusMute;
    if (!mixerCommandFailed(exitCode, timedOut)) {
        const QString text = QString::fromUtf8(out);
        bool parsedMute = false;
        const float parsed = parseVolumeLine(text, &parsedMute);
        if (parsed >= 0.0f)
            volume = parsed;
        mute = parsedMute;
    } else if (purpose == Purpose::DefaultRequest && volume < 0.0f) {
        volume = statusVolume;
    }

    runWpctl({QStringLiteral("inspect"), QString::number(id)},
             [this, purpose, id, name, description, volume,
              mute](const QByteArray &out2, const QByteArray &err2, int code2, bool to2) {
                 handleInspect(purpose, id, name, description, volume, mute, out2, err2, code2,
                               to2);
             });
}

void WpctlMixer::handleInspect(Purpose purpose, int id, const QString &fallbackName,
                               const QString &description, float volume, bool mute,
                               // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                               const QByteArray &out, const QByteArray &err, int exitCode,
                               bool timedOut)
{
    Q_UNUSED(err);
    Q_UNUSED(exitCode);
    Q_UNUSED(timedOut);

    QString name;
    int channels = 0;
    if (!mixerCommandFailed(exitCode, timedOut)) {
        const QString text = QString::fromUtf8(out);
        name = parseNodeName(text);
        channels = parseAudioChannels(text);
    }
    if (name.isEmpty())
        name = fallbackName; // defensive: cached/empty rather than a crash

    if (channels <= 0)
        channels = 2;
    if (!name.isEmpty()) {
        m_idToNodeName.insert(id, name);
        m_nameToId.insert(name, id);
    }
    if (!description.isEmpty())
        m_idToDescription.insert(id, description);

    SinkState state;
    state.index = id;
    state.name = name;
    state.description = description;
    state.volumes = QVector<int>(channels, qBound(0, qRound(volume * 65536.0f), 65536));
    state.mute = mute;
    state.state = QStringLiteral("UNKNOWN"); // wpctl status has no idle/suspend info

    switch (purpose) {
    case Purpose::DefaultRequest:
        if (!name.isEmpty() && name != m_defaultSinkName) {
            m_defaultSinkName = name;
            m_defaultSinkId = id;
            emit defaultSinkChanged(name);
        }
        m_defaultVolume = volume;
        m_defaultMute = mute;
        emit sinkReady(state);
        break;
    case Purpose::NameLookup:
        emit sinkReady(state);
        break;
    case Purpose::Poll:
        if (!name.isEmpty() && name != m_defaultSinkName) {
            m_defaultSinkName = name;
            m_defaultSinkId = id;
            emit defaultSinkChanged(name);
        }
        m_defaultVolume = volume;
        m_defaultMute = mute;
        emit sinkChanged(state);
        break;
    }
}

void WpctlMixer::setSinkVolumes(const QString &sinkName, const QVector<int> &volumes)
{
    if (volumes.isEmpty()) {
        emit volumeError(QStringLiteral("no volume values"));
        return;
    }
    const int id = resolveId(sinkName);
    if (id < 0) {
        emit volumeError(QStringLiteral("wpctl backend has not resolved sink id for ")
                         + sinkName);
        return;
    }

    // Uniform only (caps.perChannel = false) and never above 100 %: the -l
    // limit plus our own clamp means no software amplification, ever.
    const double target = qBound(0.0, volumes.first() / 65536.0, 1.0);
    runWpctl({QStringLiteral("set-volume"), QStringLiteral("-l"), QStringLiteral("1.0"),
              QString::number(id), QString::number(target, 'f', 6)},
             [this, sinkName](const QByteArray &, const QByteArray &err, int code,
                              bool timedOut) {
                 if (mixerCommandFailed(code, timedOut))
                     emit volumeError(mixerFailureReason(err, code, timedOut));
                 else
                     emit volumeApplied(sinkName);
             });
}

void WpctlMixer::setSinkMute(const QString &sinkName, bool mute)
{
    const int id = resolveId(sinkName);
    if (id < 0) {
        emit muteError(QStringLiteral("wpctl backend has not resolved sink id for ") + sinkName);
        return;
    }
    runWpctl({QStringLiteral("set-mute"), QString::number(id),
              mute ? QStringLiteral("true") : QStringLiteral("false")},
             [this, sinkName](const QByteArray &, const QByteArray &err, int code,
                              bool timedOut) {
                 if (mixerCommandFailed(code, timedOut))
                     emit muteError(mixerFailureReason(err, code, timedOut));
                 else
                     emit muteApplied(sinkName);
             });
}

void WpctlMixer::listSinkInputs()
{
    emit sinkInputsError(QStringLiteral("wpctl backend cannot list audio streams"));
}

void WpctlMixer::setSinkInputVolume(int, const QVector<int> &)
{
    emit sinkInputVolumeError(QStringLiteral("wpctl backend cannot control audio streams"));
}

void WpctlMixer::setSinkInputMute(int, bool)
{
    emit sinkInputMuteError(QStringLiteral("wpctl backend cannot control audio streams"));
}

void WpctlMixer::startWatch()
{
    m_pollTimer->start(); // (re)starts the interval
    fetchDefault(Purpose::Poll);
}

void WpctlMixer::stopWatch()
{
    m_pollTimer->stop();
    m_backend->cancelAll();
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

QVector<WpctlMixer::WpSink> WpctlMixer::parseStatus(const QString &output)
{
    // Real capture (Phase 0 / live):
    //   │  *   50. Built-in Audio Analog Stereo        [vol: 0.61]
    //   │  *   51. Built-in Audio Analog Stereo        [vol: 0.00 MUTED]
    QVector<WpSink> sinks;
    WpSink current;
    bool inAudio = false;
    bool inSinks = false;

    // The tree prefix (│, spaces) is not whitespace, so trim() keeps it:
    // `.*?` skips it while still capturing the default-sink `*`.
    static const QRegularExpression sinkLine(
        QStringLiteral(R"(^.*?(\*)?\s*(\d+)\.\s+(.+?)\s+\[vol:\s*([\d.]+)(\s+MUTED)?\]\s*$)"));

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();

        if (line.startsWith(QLatin1String("Audio"))) {
            inAudio = true;
            inSinks = false;
            continue;
        }
        if (line.startsWith(QLatin1String("Video")) || line.startsWith(QLatin1String("Settings"))) {
            inAudio = false;
            inSinks = false;
            continue;
        }
        if (!inAudio)
            continue;

        if (line.contains(QLatin1String("Sinks:"))) {
            inSinks = true;
            continue;
        }
        if (line.contains(QLatin1String("Sources:")) || line.contains(QLatin1String("Filters:"))
            || line.contains(QLatin1String("Streams:"))) {
            inSinks = false;
            continue;
        }
        if (!inSinks)
            continue;

        const QRegularExpressionMatch match = sinkLine.match(line);
        if (!match.hasMatch())
            continue;

        if (current.id != -1)
            sinks.append(current);
        current = WpSink();
        current.id = match.captured(2).toInt();
        current.description = match.captured(3).trimmed();
        current.volume = match.captured(4).toFloat();
        current.mute = !match.captured(5).isEmpty();
        current.isDefault = !match.captured(1).isEmpty();
    }
    if (current.id != -1)
        sinks.append(current);

    return sinks;
}

float WpctlMixer::parseVolumeLine(const QString &output, bool *muted)
{
    // `wpctl get-volume 50` -> "Volume: 0.61" / "Volume: 0.61 [MUTED]"
    if (muted)
        *muted = output.contains(QLatin1String("[MUTED]"));

    static const QRegularExpression volumeLine(
        QStringLiteral(R"(Volume:\s*([\d.]+))"));
    const QRegularExpressionMatch match = volumeLine.match(output);
    if (match.hasMatch())
        return match.captured(1).toFloat();
    return -1.0f; // caller keeps its fallback value
}

QString WpctlMixer::parseNodeName(const QString &output)
{
    // `wpctl inspect 50` -> `  * node.name = "alsa_output.pci-..."`
    static const QRegularExpression nodeName(
        QStringLiteral(R"RE(node\.name\s*=\s*"([^"]+)")RE"));
    const QRegularExpressionMatch match = nodeName.match(output);
    if (match.hasMatch())
        return match.captured(1);
    return {};
}

int WpctlMixer::parseAudioChannels(const QString &output)
{
    // `wpctl inspect 50` -> `    audio.channels = "2"`
    static const QRegularExpression channels(
        QStringLiteral(R"RE(audio\.channels\s*=\s*"(\d+)")RE"));
    const QRegularExpressionMatch match = channels.match(output);
    if (match.hasMatch())
        return match.captured(1).toInt();
    return 0;
}
