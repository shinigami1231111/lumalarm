#include "audio/PactlMixer.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace {

// Phase 0: plain `pactl info` only connects when the server is passed
// explicitly (or when running as the desktop user where the socket is the
// default). The app runs as the desktop user, but resolving the socket here
// keeps us working in either situation. An explicit PULSE_SERVER wins.
QString resolvePulseServer()
{
    QString envServer = qEnvironmentVariable("PULSE_SERVER");
    if (!envServer.isEmpty())
        return envServer;

    const QString xdg = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!xdg.isEmpty() && QFile::exists(xdg + QLatin1String("/pulse/native")))
        return QStringLiteral("unix:") + xdg + QLatin1String("/pulse/native");

    return {};
}

int indexAfterHash(const QString &message)
{
    const qsizetype hash = message.indexOf(QLatin1Char('#'));
    if (hash < 0)
        return -1;
    bool ok = false;
    const int index = message.mid(hash + 1).section(QLatin1Char(' '), 0, 0).toInt(&ok);
    return ok ? index : -1;
}

} // namespace

PactlMixer::PactlMixer(QObject *parent)
    : SystemMixer(parent)
    , m_pulseServer(resolvePulseServer())
    , m_backend(new MixerBackend(this))
{
    m_subscribeProc = new QProcess(this);
    m_subscribeProc->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_subscribeProc->setProcessEnvironment(env);

    connect(m_subscribeProc, &QProcess::readyReadStandardOutput, this, [this]() {
        while (m_subscribeProc->canReadLine())
            onSubscribeLine(m_subscribeProc->readLine());
    });
    // A dead subscribe process is restarted only while watching is wanted
    // (stopWatch() flips the flag before killing it).
    connect(m_subscribeProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                if (m_watchWanted)
                    QTimer::singleShot(1000, this, &PactlMixer::startWatch);
            });
    connect(m_subscribeProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            qWarning() << "Lumalarm: pactl subscribe failed to start";
    });
}

PactlMixer::~PactlMixer()
{
    stopWatch();
}

QStringList PactlMixer::pactlArgsSuffix() const
{
    QStringList prefix;
    if (!m_pulseServer.isEmpty())
        prefix << QStringLiteral("--server") << m_pulseServer;
    return prefix;
}

void PactlMixer::runPactl(const QStringList &args, MixerBackend::Callback callback)
{
    QStringList full = pactlArgsSuffix();
    full << args;
    m_backend->enqueue(QStringLiteral("pactl"), full, std::move(callback));
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

void PactlMixer::detect()
{
    runPactl({QStringLiteral("info")},
             [this](const QByteArray &out, const QByteArray &err, int code, bool timedOut) {
                 handleDetect(out, err, code, timedOut);
             });
}

void PactlMixer::handleDetect(const QByteArray &, const QByteArray &err, int exitCode,
                              bool timedOut)
{
    if (mixerCommandFailed(exitCode, timedOut)) {
        // Present but unusable (no Pulse socket, hung server, ...): report
        // empty capabilities so the controller falls through to wpctl.
        qWarning() << "Lumalarm: pactl detect failed:"
                   << mixerFailureReason(err, exitCode, timedOut);
        emit backendDetected(QStringLiteral("pactl"), MixerCapabilities{});
        return;
    }

    MixerCapabilities caps;
    caps.canSetVolume = true;
    caps.canMute = true;
    caps.perChannel = true;
    caps.hasSinkInputs = true;
    caps.canSubscribe = true;
    emit backendDetected(QStringLiteral("pactl"), caps);
}

void PactlMixer::getDefaultSink()
{
    runPactl({QStringLiteral("--format=json"), QStringLiteral("info")},
             [this](const QByteArray &out, const QByteArray &err, int code, bool timedOut) {
                 handleInfo(out, err, code, timedOut);
             });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void PactlMixer::handleInfo(const QByteArray &out, const QByteArray &err, int exitCode,
                            bool timedOut)
{
    if (mixerCommandFailed(exitCode, timedOut)) {
        // Older pactl without JSON info: fall back to the plain text form.
        runPactl({QStringLiteral("info")},
                 [this](const QByteArray &out2, const QByteArray &err2, int code2, bool to2) {
                     handleInfoText(out2, err2, code2, to2);
                 });
        return;
    }

    const QString name = parseInfoDefaultSink(out);
    if (name.isEmpty()) {
        runPactl({QStringLiteral("info")},
                 [this](const QByteArray &out2, const QByteArray &err2, int code2, bool to2) {
                     handleInfoText(out2, err2, code2, to2);
                 });
        return;
    }

    fetchSinks(ListPurpose::DefaultSink, name, -1);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void PactlMixer::handleInfoText(const QByteArray &out, const QByteArray &err, int exitCode,
                                bool timedOut)
{
    if (mixerCommandFailed(exitCode, timedOut)) {
        emit sinkError(mixerFailureReason(err, exitCode, timedOut));
        return;
    }

    const QString name = parseInfoDefaultSink(out);
    if (name.isEmpty()) {
        emit sinkError(QStringLiteral("pactl reports no default sink"));
        return;
    }

    fetchSinks(ListPurpose::DefaultSink, name, -1);
}

void PactlMixer::getSink(const QString &name)
{
    fetchSinks(ListPurpose::LookupByName, name, -1);
}

void PactlMixer::listSinks()
{
    fetchSinks(ListPurpose::AllDevices, QString(), -1);
}

void PactlMixer::moveSinkInput(int index, const QString &sinkName)
{
    if (index < 0 || sinkName.isEmpty()) {
        emit moveError(QStringLiteral("invalid stream or sink"));
        return;
    }
    QStringList args{QStringLiteral("move-sink-input"), QString::number(index),
                     sinkName};
    args << pactlArgsSuffix();
    runPactl(args, [this, index](const QByteArray &, const QByteArray &err,
                                 int code, bool timedOut) {
        if (!mixerCommandFailed(code, timedOut)) {
            emit moveApplied(index);
            return;
        }
        emit moveError(mixerFailureReason(err, code, timedOut));
    });
}

void PactlMixer::fetchSinks(ListPurpose purpose, const QString &wantedName, int wantedIndex)
{
    runPactl({QStringLiteral("--format=json"), QStringLiteral("list"), QStringLiteral("sinks")},
             [this, purpose, wantedName, wantedIndex](const QByteArray &out,
                                                      const QByteArray &err, int code,
                                                      bool timedOut) {
                 handleSinkList(purpose, wantedName, wantedIndex, out, err, code, timedOut);
             });
}

void PactlMixer::handleSinkList(ListPurpose purpose, const QString &wantedName, int wantedIndex,
                                // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                                const QByteArray &out, const QByteArray &err, int exitCode,
                                bool timedOut)
{
    const auto background = (purpose == ListPurpose::ChangedSink);

    if (mixerCommandFailed(exitCode, timedOut)) {
        const QString reason = mixerFailureReason(err, exitCode, timedOut);
        if (background)
            qWarning() << "Lumalarm: sink refresh failed:" << reason;
        else if (purpose == ListPurpose::AllDevices)
            emit sinksError(reason);
        else
            emit sinkError(reason);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(out, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        // Defensive parsing: unparsable output degrades, never crashes.
        if (background)
            qWarning() << "Lumalarm: unexpected pactl list sinks output";
        else if (purpose == ListPurpose::AllDevices)
            emit sinksError(QStringLiteral("unexpected pactl list sinks output"));
        else
            emit sinkError(QStringLiteral("unexpected pactl list sinks output"));
        return;
    }

    QVector<SinkState> sinks;
    sinks.reserve(doc.array().size());
    for (const auto &value : doc.array()) {
        if (value.isObject())
            sinks.append(parseSink(value.toObject()));
    }
    if (sinks.isEmpty()) {
        // No devices is a valid (if surprising) answer for the device list.
        if (purpose == ListPurpose::AllDevices) {
            emit sinksReady({});
            return;
        }
        if (!background)
            emit sinkError(QStringLiteral("no sinks found"));
        return;
    }

    switch (purpose) {
    case ListPurpose::DefaultSink: {
        const SinkState *chosen = nullptr;
        for (const SinkState &sink : sinks) {
            if (!wantedName.isEmpty() && sink.name == wantedName) {
                chosen = &sink;
                break;
            }
        }
        if (!chosen) { // name mismatch (hotplug race): fall back to a running sink
            for (const SinkState &sink : sinks) {
                if (sink.state == QLatin1String("RUNNING")) {
                    chosen = &sink;
                    break;
                }
            }
        }
        if (!chosen)
            chosen = &sinks.first();

        if (chosen->name != m_defaultSinkName) {
            m_defaultSinkName = chosen->name;
            emit defaultSinkChanged(chosen->name);
        }
        emit sinkReady(*chosen);
        break;
    }
    case ListPurpose::LookupByName: {
        for (const SinkState &sink : sinks) {
            if (sink.name == wantedName) {
                emit sinkReady(sink);
                return;
            }
        }
        emit sinkError(QStringLiteral("sink not found: ") + wantedName);
        break;
    }
    case ListPurpose::ChangedSink: {
        for (const SinkState &sink : sinks) {
            if (sink.index == wantedIndex) {
                emit sinkChanged(sink);
                return;
            }
        }
        break; // sink vanished: nothing to report
    }
    case ListPurpose::AllDevices:
        emit sinksReady(QList<SinkState>(sinks.begin(), sinks.end()));
        break;
    }
}

void PactlMixer::setSinkVolumes(const QString &sinkName, const QVector<int> &volumes)
{
    if (volumes.isEmpty()) {
        emit volumeError(QStringLiteral("no volume values"));
        return;
    }

    QStringList values;
    values.reserve(volumes.size());
    for (int volume : volumes)
        values << QString::number(volume);

    QStringList args{QStringLiteral("set-sink-volume"), sinkName};
    args << values;
    runPactl(args,
             [this, sinkName](const QByteArray &, const QByteArray &err, int code,
                              bool timedOut) {
                 if (mixerCommandFailed(code, timedOut))
                     emit volumeError(mixerFailureReason(err, code, timedOut));
                 else
                     emit volumeApplied(sinkName);
             });
}

void PactlMixer::setSinkMute(const QString &sinkName, bool mute)
{
    runPactl({QStringLiteral("set-sink-mute"), sinkName, mute ? QStringLiteral("true")
                                                              : QStringLiteral("false")},
             [this, sinkName](const QByteArray &, const QByteArray &err, int code,
                              bool timedOut) {
                 if (mixerCommandFailed(code, timedOut))
                     emit muteError(mixerFailureReason(err, code, timedOut));
                 else
                     emit muteApplied(sinkName);
             });
}

void PactlMixer::listSinkInputs()
{
    const int wantedInputIndex = -1; // no specific stream to report back as changed
    runPactl({QStringLiteral("--format=json"), QStringLiteral("list"),
              QStringLiteral("sink-inputs")},
             [this, wantedInputIndex](const QByteArray &out, const QByteArray &err, int code,
                                      bool timedOut) {
                 handleSinkInputs(wantedInputIndex, out, err, code, timedOut);
             });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void PactlMixer::handleSinkInputs(int wantedInputIndex, const QByteArray &out,
                                  const QByteArray &err, int exitCode, bool timedOut)
{
    if (mixerCommandFailed(exitCode, timedOut)) {
        emit sinkInputsError(mixerFailureReason(err, exitCode, timedOut));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(out, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        emit sinkInputsError(QStringLiteral("unexpected pactl list sink-inputs output"));
        return;
    }

    QVector<SinkInputInfo> inputs;
    inputs.reserve(doc.array().size());
    for (const auto &value : doc.array()) {
        if (value.isObject())
            inputs.append(parseSinkInput(value.toObject()));
    }
    emit sinkInputsReady(inputs);

    if (wantedInputIndex >= 0) {
        for (const SinkInputInfo &input : inputs) {
            if (input.index == wantedInputIndex) {
                emit sinkInputChanged(input);
                break;
            }
        }
    }
}

void PactlMixer::setSinkInputVolume(int index, const QVector<int> &volumes)
{
    if (volumes.isEmpty()) {
        emit sinkInputVolumeError(QStringLiteral("no volume values"));
        return;
    }

    QStringList values;
    values.reserve(volumes.size());
    for (int volume : volumes)
        values << QString::number(volume);

    QStringList args{QStringLiteral("set-sink-input-volume"), QString::number(index)};
    args << values;
    runPactl(args,
             [this, index](const QByteArray &, const QByteArray &err, int code, bool timedOut) {
                 if (mixerCommandFailed(code, timedOut))
                     emit sinkInputVolumeError(mixerFailureReason(err, code, timedOut));
                 else
                     emit sinkInputVolumeApplied(index);
             });
}

void PactlMixer::setSinkInputMute(int index, bool mute)
{
    runPactl({QStringLiteral("set-sink-input-mute"), QString::number(index),
              mute ? QStringLiteral("true") : QStringLiteral("false")},
             [this, index](const QByteArray &, const QByteArray &err, int code, bool timedOut) {
                 if (mixerCommandFailed(code, timedOut))
                     emit sinkInputMuteError(mixerFailureReason(err, code, timedOut));
                 else
                     emit sinkInputMuteApplied(index);
             });
}

// ---------------------------------------------------------------------------
// Watch (pactl subscribe)
// ---------------------------------------------------------------------------

void PactlMixer::startWatch()
{
    m_watchWanted = true;
    if (m_subscribeProc->state() != QProcess::NotRunning)
        return;

    QStringList args = pactlArgsSuffix();
    args << QStringLiteral("subscribe");
    m_subscribeProc->start(QStringLiteral("pactl"), args);
}

void PactlMixer::stopWatch()
{
    m_watchWanted = false;
    if (m_subscribeProc->state() != QProcess::NotRunning) {
        m_subscribeProc->kill();
        m_subscribeProc->waitForFinished(500);
    }
    m_backend->cancelAll();
}

void PactlMixer::onSubscribeLine(const QByteArray &line)
{
    const QString message = QString::fromUtf8(line).trimmed();
    if (!message.startsWith(QLatin1String("Event ")))
        return;

    if (message.contains(QLatin1String(" on sink-input #"))) {
        const int index = indexAfterHash(message);
        if (index < 0)
            return;
        // Re-list streams and report the one that changed.
        runPactl({QStringLiteral("--format=json"), QStringLiteral("list"),
                  QStringLiteral("sink-inputs")},
                 [this, index](const QByteArray &out, const QByteArray &err, int code,
                               bool timedOut) {
                     handleSinkInputs(index, out, err, code, timedOut);
                 });
    } else if (message.contains(QLatin1String(" on sink #"))) {
        const int index = indexAfterHash(message);
        if (index < 0)
            return;
        fetchSinks(ListPurpose::ChangedSink, {}, index);
    } else if (message.contains(QLatin1String(" on card #"))
               || message.contains(QLatin1String(" on server #"))) {
        // Port/profile switches and default-sink changes hide in card/server
        // events, so refresh the default sink (may emit defaultSinkChanged).
        getDefaultSink();
    }
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

SinkState PactlMixer::parseSink(const QJsonObject &object)
{
    SinkState sink;
    sink.index = object.value(QStringLiteral("index")).toInt(-1);
    sink.name = object.value(QStringLiteral("name")).toString();
    sink.description = object.value(QStringLiteral("description")).toString();
    sink.mute = object.value(QStringLiteral("mute")).toBool(false);
    sink.state = object.value(QStringLiteral("state")).toString(QStringLiteral("UNKNOWN"));
    if (sink.state.isEmpty())
        sink.state = QStringLiteral("UNKNOWN");

    const QString channelMap = object.value(QStringLiteral("channel_map")).toString();
    sink.volumes = parseVolumes(object.value(QStringLiteral("volume")).toObject(), channelMap);

    const QJsonObject sinkProperties = object.value(QStringLiteral("properties")).toObject();
    const QJsonArray ports = object.value(QStringLiteral("ports")).toArray();
    for (const auto &value : ports) {
        const QJsonObject portObject = value.toObject();
        SinkPort port;
        port.name = portObject.value(QStringLiteral("name")).toString();
        port.description = portObject.value(QStringLiteral("description")).toString();
        port.type = portType(portObject.value(QStringLiteral("type")).toString(), sinkProperties);
        // "available" / "not available" / "availability unknown" — anything
        // but an explicit "not available" counts as potentially usable.
        port.available =
            portObject.value(QStringLiteral("availability")).toString()
            != QLatin1String("not available");
        port.priority = portObject.value(QStringLiteral("priority")).toInt(0);
        sink.ports.append(port);
    }

    const QString activePortName = object.value(QStringLiteral("active_port")).toString();
    for (const SinkPort &port : sink.ports) {
        if (port.name == activePortName) {
            sink.activePort = port;
            break;
        }
    }

    return sink;
}

SinkInputInfo PactlMixer::parseSinkInput(const QJsonObject &object)
{
    SinkInputInfo input;
    input.index = object.value(QStringLiteral("index")).toInt(-1);
    input.sinkIndex = object.value(QStringLiteral("sink")).toInt(-1);
    input.corked = object.value(QStringLiteral("corked")).toBool(false);
    input.mute = object.value(QStringLiteral("mute")).toBool(false);

    const QJsonObject properties = object.value(QStringLiteral("properties")).toObject();
    input.applicationName = properties.value(QStringLiteral("application.name")).toString();
    input.nodeName = properties.value(QStringLiteral("node.name")).toString();

    const QJsonValue pid = properties.value(QStringLiteral("application.process.id"));
    if (pid.isDouble()) {
        input.processId = static_cast<qint64>(pid.toDouble());
    } else if (pid.isString()) {
        bool ok = false;
        const qint64 parsed = pid.toString().toLongLong(&ok);
        input.processId = ok ? parsed : -1;
    }

    const QString channelMap = object.value(QStringLiteral("channel_map")).toString();
    input.volumes = parseVolumes(object.value(QStringLiteral("volume")).toObject(), channelMap);

    return input;
}

QString PactlMixer::parseInfoDefaultSink(const QByteArray &infoOutput)
{
    // Preferred: `pactl --format=json info` (Phase 0 capture).
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(infoOutput, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QString name =
            doc.object().value(QStringLiteral("default_sink_name")).toString();
        if (!name.isEmpty())
            return name;
    }

    // Fallback: plain text "Default Sink: <name>" line.
    const QString text = QString::fromUtf8(infoOutput);
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("Default Sink: ")))
            return line.mid(13).trimmed();
    }

    return {};
}

QString PactlMixer::portType(const QString &type, const QJsonObject &properties)
{
    const QString t = type.toLower();
    if (t.contains(QLatin1String("speaker")))
        return QStringLiteral("speaker");
    if (t.contains(QLatin1String("headphone")))
        return QStringLiteral("headphones");
    if (t.contains(QLatin1String("headset")))
        return QStringLiteral("headset");
    if (t.contains(QLatin1String("bluetooth")))
        return QStringLiteral("bluetooth");
    if (t.contains(QLatin1String("hdmi")) || t.contains(QLatin1String("displayport")))
        return QStringLiteral("hdmi");
    if (t.contains(QLatin1String("line")))
        return QStringLiteral("speaker"); // line-out feeds speakers in practice

    // Fall back to device properties when the port type is unhelpful.
    const QString bus = properties.value(QStringLiteral("device.bus")).toString().toLower();
    if (bus == QLatin1String("bluetooth"))
        return QStringLiteral("bluetooth");

    const QString form = properties.value(QStringLiteral("device.form_factor")).toString().toLower();
    if (form == QLatin1String("headphone"))
        return QStringLiteral("headphones");
    if (form == QLatin1String("headset"))
        return QStringLiteral("headset");
    if (form == QLatin1String("speaker"))
        return QStringLiteral("speaker");
    if (form == QLatin1String("tv") || form == QLatin1String("monitor"))
        return QStringLiteral("hdmi");

    return QStringLiteral("unknown");
}

QVector<int> PactlMixer::parseVolumes(const QJsonObject &volumeObject, const QString &channelMap)
{
    QVector<int> volumes;

    // Channel-map order is the order pactl's set-sink-volume assigns values
    // in, so get/set round-trips must use it (QJsonObject keys would come out
    // alphabetically sorted and shuffle multi-channel sinks).
    const QStringList channels =
        channelMap.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &rawChannel : channels) {
        const QString channel = rawChannel.trimmed();
        const auto it = volumeObject.constFind(channel);
        if (it != volumeObject.constEnd())
            volumes.append(parseVolumeValue(it.value()));
    }
    if (!volumes.isEmpty())
        return volumes;

    // Defensive fallback: no usable channel map — read whatever is there.
    for (const QString &key : volumeObject.keys())
        volumes.append(parseVolumeValue(volumeObject.value(key)));
    return volumes;
}

int PactlMixer::parseVolumeValue(const QJsonValue &value)
{
    // pactl JSON: {"value": 52429, "value_percent": "80%", "db": "-5.81 dB"}
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.contains(QStringLiteral("value")))
            return object.value(QStringLiteral("value")).toInt(0);
    }
    if (value.isDouble())
        return value.toInt(0); // tolerate flat integers
    return 0;
}
