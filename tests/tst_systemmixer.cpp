#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

#include "systemmixer_fixtures.h"

#include "audio/AmixerMixer.h"
#include "audio/FakeMixer.h"
#include "audio/MixerBackend.h"
#include "audio/NullMixer.h"
#include "audio/PactlMixer.h"
#include "audio/SystemMixer.h"
#include "audio/WpctlMixer.h"

class TestSystemMixer : public QObject
{
    Q_OBJECT

private slots:
    // --- MixerBackend: async runner, LC_ALL=C, timeouts, FIFO, no shell ---
    void backend_capturesOutputAsynchronously();
    void backend_passesArgumentsWithoutShell();
    void backend_setsLocaleC();
    void backend_timesOutHungCommand();
    void backend_reportsMissingBinary();
    void backend_completesInFifoOrder();

    // --- FakeMixer: deterministic double ---------------------------------
    void fakeMixer_detect();
    void fakeMixer_volumeApplyAndFailure();
    void fakeMixer_muteApplyAndFailure();
    void fakeMixer_streams();
    void fakeMixer_deviceListAndMove();

    // --- PactlMixer parsers ----------------------------------------------
    void pactl_parseSinkStereoWithPorts();
    void pactl_parseSink51ChannelOrder();
    void pactl_parseSinkInput();
    void pactl_parseInfoJsonAndText();
    void pactl_portType();

    // --- WpctlMixer parsers ----------------------------------------------
    void wpctl_parseStatus();
    void wpctl_parseStatusMutedAndSectionScoping();
    void wpctl_parseInspect();
    void wpctl_moveWithoutResolvedIdFails();

    // --- AmixerMixer parser ----------------------------------------------
    void amixer_parseStereoMonoAndGarbage();

    // --- Fallback & factory ----------------------------------------------
    void nullMixer_degradesGracefully();
    void backendsWithoutEnumerationReportUnavailable();
    void factory_picksBackend();
};

// ---------------------------------------------------------------------------
// MixerBackend
// ---------------------------------------------------------------------------

void TestSystemMixer::backend_capturesOutputAsynchronously()
{
    MixerBackend backend;
    bool done = false;
    QByteArray out;
    int code = -2;

    backend.enqueue(QStandardPaths::findExecutable(QStringLiteral("echo")),
                    {QStringLiteral("hello")},
                    [&](const QByteArray &stdOut, const QByteArray &, int exitCode,
                        bool timedOut) {
                        QVERIFY(!timedOut);
                        out = stdOut;
                        code = exitCode;
                        done = true;
                    });

    // Asynchronous: nothing may have completed before the event loop runs.
    QVERIFY(!done);
    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
    QCOMPARE(code, 0);
    QCOMPARE(QString::fromUtf8(out).trimmed(), QStringLiteral("hello"));
    QVERIFY(!backend.isBusy());
}

void TestSystemMixer::backend_passesArgumentsWithoutShell()
{
    MixerBackend backend;
    QVector<QByteArray> out;

    // If a shell were involved, "; echo pwned" would run as a command and
    // the output would differ.
    backend.enqueue(QStandardPaths::findExecutable(QStringLiteral("echo")),
                    {QStringLiteral("a; echo pwned"), QStringLiteral("b")},
                    [&](const QByteArray &stdOut, const QByteArray &, int, bool) {
                        out.append(stdOut);
                    });

    QTRY_COMPARE_WITH_TIMEOUT(out.size(), 1, 5000);
    QCOMPARE(QString::fromUtf8(out.first()).trimmed(),
             QStringLiteral("a; echo pwned b"));
}

void TestSystemMixer::backend_setsLocaleC()
{
    MixerBackend backend;
    QVector<QByteArray> out;

    backend.enqueue(QStandardPaths::findExecutable(QStringLiteral("env")), {},
                    [&](const QByteArray &stdOut, const QByteArray &, int, bool) {
                        out.append(stdOut);
                    });

    QTRY_COMPARE_WITH_TIMEOUT(out.size(), 1, 5000);
    QVERIFY(QString::fromUtf8(out.first()).contains(QStringLiteral("LC_ALL=C")));
}

void TestSystemMixer::backend_timesOutHungCommand()
{
    MixerBackend backend;
    bool done = false;
    bool wasTimedOut = false;
    QElapsedTimer timer;
    timer.start();

    backend.enqueue(QStandardPaths::findExecutable(QStringLiteral("sleep")),
                    {QStringLiteral("3")},
                    [&](const QByteArray &, const QByteArray &, int, bool timedOut) {
                        wasTimedOut = timedOut;
                        done = true;
                    },
                    150);

    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
    // The 150 ms timeout must have killed the 3 s sleep well before it ends.
    QVERIFY(timer.elapsed() < 2500);
    QVERIFY(wasTimedOut);
}

void TestSystemMixer::backend_reportsMissingBinary()
{
    MixerBackend backend;
    bool done = false;
    int code = 0;
    QByteArray err;

    backend.enqueue(QStringLiteral("/nonexistent/lumalarm-missing-binary"), {},
                    [&](const QByteArray &, const QByteArray &stdErr, int exitCode,
                        bool) {
                        err = stdErr;
                        code = exitCode;
                        done = true;
                    });

    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
    QCOMPARE(code, -1);
    QVERIFY(QString::fromUtf8(err).contains(QStringLiteral("failed to start")));
}

void TestSystemMixer::backend_completesInFifoOrder()
{
    MixerBackend backend;
    QVector<QByteArray> outputs;

    for (const char *label : {"first", "second", "third"}) {
        backend.enqueue(QStandardPaths::findExecutable(QStringLiteral("echo")),
                        {QString::fromLatin1(label)},
                        [&](const QByteArray &stdOut, const QByteArray &, int, bool) {
                            outputs.append(stdOut);
                        });
    }

    QTRY_COMPARE_WITH_TIMEOUT(outputs.size(), 3, 5000);
    QCOMPARE(QString::fromUtf8(outputs.at(0)).trimmed(), QStringLiteral("first"));
    QCOMPARE(QString::fromUtf8(outputs.at(1)).trimmed(), QStringLiteral("second"));
    QCOMPARE(QString::fromUtf8(outputs.at(2)).trimmed(), QStringLiteral("third"));
}

// ---------------------------------------------------------------------------
// FakeMixer
// ---------------------------------------------------------------------------

void TestSystemMixer::fakeMixer_detect()
{
    FakeMixer mixer;
    QSignalSpy detected(&mixer, &SystemMixer::backendDetected);
    mixer.detect();

    QCOMPARE(detected.count(), 1);
    const auto args = detected.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("fake"));
    const MixerCapabilities caps = args.at(1).value<MixerCapabilities>();
    QVERIFY(caps.canSetVolume);
    QVERIFY(caps.canMute);
    QVERIFY(caps.perChannel);
    QVERIFY(caps.hasSinkInputs);
    QVERIFY(caps.canSubscribe);
}

void TestSystemMixer::fakeMixer_volumeApplyAndFailure()
{
    FakeMixer mixer;
    QSignalSpy applied(&mixer, &SystemMixer::volumeApplied);
    QSignalSpy failed(&mixer, &SystemMixer::volumeError);

    mixer.setSinkVolumes(QStringLiteral("fake-sink"), {32768, 16384});
    QCOMPARE(applied.count(), 1);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(mixer.applyVolumeCallCount(), 1);
    QCOMPARE(mixer.lastAppliedVolumes(), QVector<int>({32768, 16384}));
    QCOMPARE(mixer.sink().volumes, QVector<int>({32768, 16384}));

    mixer.setNextVolumeResult(false);
    mixer.setSinkVolumes(QStringLiteral("fake-sink"), {1000, 1000});
    QCOMPARE(failed.count(), 1);
    QCOMPARE(mixer.applyVolumeCallCount(), 2);
    // Failed apply must not touch the sink state.
    QCOMPARE(mixer.sink().volumes, QVector<int>({32768, 16384}));
}

void TestSystemMixer::fakeMixer_muteApplyAndFailure()
{
    FakeMixer mixer;
    QSignalSpy applied(&mixer, &SystemMixer::muteApplied);
    QSignalSpy failed(&mixer, &SystemMixer::muteError);

    mixer.setSinkMute(QStringLiteral("fake-sink"), true);
    QCOMPARE(applied.count(), 1);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(mixer.applyMuteCallCount(), 1);
    QVERIFY(mixer.lastAppliedMute());
    QVERIFY(mixer.sink().mute);

    mixer.setNextMuteResult(false);
    mixer.setSinkMute(QStringLiteral("fake-sink"), false);
    QCOMPARE(failed.count(), 1);
    QVERIFY(mixer.sink().mute); // unchanged after failure
}

void TestSystemMixer::fakeMixer_streams()
{
    FakeMixer mixer;
    SinkInputInfo input;
    input.index = 42;
    input.sinkIndex = 50;
    input.applicationName = QStringLiteral("lumalarm");
    input.nodeName = QStringLiteral("lumalarm");
    input.processId = 1234;
    input.volumes = {65536, 65536};
    mixer.addSinkInput(input);

    QSignalSpy listed(&mixer, &SystemMixer::sinkInputsReady);
    mixer.listSinkInputs();
    QCOMPARE(listed.count(), 1);
    const QVector<SinkInputInfo> inputs =
        listed.takeFirst().at(0).value<QVector<SinkInputInfo>>();
    QCOMPARE(inputs.size(), 1);
    QCOMPARE(inputs.first().applicationName, QStringLiteral("lumalarm"));
    QCOMPARE(inputs.first().processId, 1234);

    QSignalSpy volumeApplied(&mixer, &SystemMixer::sinkInputVolumeApplied);
    QSignalSpy volumeFailed(&mixer, &SystemMixer::sinkInputVolumeError);
    mixer.setSinkInputVolume(42, {16384, 16384});
    QCOMPARE(volumeApplied.count(), 1);
    QCOMPARE(volumeFailed.count(), 0);
    QCOMPARE(mixer.lastSinkInputIndex(), 42);
    QCOMPARE(mixer.lastSinkInputVolumes(), QVector<int>({16384, 16384}));

    mixer.setSinkInputVolume(99, {16384, 16384}); // unknown stream
    QCOMPARE(volumeFailed.count(), 1);

    QSignalSpy changed(&mixer, &SystemMixer::sinkInputChanged);
    SinkInputInfo updated = input;
    updated.mute = true;
    mixer.simulateSinkInputChange(updated);
    QCOMPARE(changed.count(), 1);
}

void TestSystemMixer::fakeMixer_deviceListAndMove()
{
    FakeMixer mixer;
    SinkState speakers;
    speakers.index = 50;
    speakers.name = QStringLiteral("fake-speakers");
    speakers.description = QStringLiteral("Built-in Audio");
    SinkState hdmi;
    hdmi.index = 52;
    hdmi.name = QStringLiteral("fake-hdmi");
    hdmi.description = QStringLiteral("HDMI");
    mixer.setDevices({speakers, hdmi});

    QSignalSpy listed(&mixer, &SystemMixer::sinksReady);
    mixer.listSinks();
    QCOMPARE(mixer.listSinksCallCount(), 1);
    QCOMPARE(listed.count(), 1);
    const QList<SinkState> sinks =
        listed.takeFirst().at(0).value<QList<SinkState>>();
    QCOMPARE(sinks.size(), 2);
    QCOMPARE(sinks.at(0).name, QStringLiteral("fake-speakers"));
    QCOMPARE(sinks.at(1).description, QStringLiteral("HDMI"));

    QSignalSpy moved(&mixer, &SystemMixer::moveApplied);
    QSignalSpy moveFailed(&mixer, &SystemMixer::moveError);
    mixer.moveSinkInput(7, QStringLiteral("fake-hdmi"));
    QCOMPARE(mixer.moveCallCount(), 1);
    QCOMPARE(mixer.lastMovedIndex(), 7);
    QCOMPARE(mixer.lastMovedSink(), QStringLiteral("fake-hdmi"));
    QCOMPARE(moved.count(), 1);
    QCOMPARE(moveFailed.count(), 0);

    mixer.setNextMoveResult(false);
    mixer.moveSinkInput(9, QStringLiteral("unplugged"));
    QCOMPARE(moveFailed.count(), 1);
    QCOMPARE(moveFailed.first().at(0).toString(),
             QStringLiteral("fake move failure"));
}

// ---------------------------------------------------------------------------
// PactlMixer parsers
// ---------------------------------------------------------------------------

void TestSystemMixer::pactl_parseSinkStereoWithPorts()
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(kPactlSinksStereo));
    QVERIFY(doc.isArray());
    QVERIFY(!doc.array().isEmpty());

    const SinkState sink = PactlMixer::parseSink(doc.array().first().toObject());
    QCOMPARE(sink.index, 50);
    QCOMPARE(sink.name, QStringLiteral("alsa_output.pci-0000_00_1b.0.analog-stereo"));
    QCOMPARE(sink.description, QStringLiteral("Built-in Audio Analog Stereo"));
    QCOMPARE(sink.state, QStringLiteral("SUSPENDED"));
    QVERIFY(!sink.mute);
    QCOMPARE(sink.volumes, QVector<int>({52429, 52429})); // 80 % of 65536

    QCOMPARE(sink.ports.size(), 2);
    QCOMPARE(sink.ports.at(0).type, QStringLiteral("speaker"));
    QVERIFY(sink.ports.at(0).available);
    QCOMPARE(sink.ports.at(1).type, QStringLiteral("headphones"));
    QVERIFY(!sink.ports.at(1).available);

    QCOMPARE(sink.activePort.name, QStringLiteral("analog-output-speaker"));
    QCOMPARE(sink.activePort.type, QStringLiteral("speaker"));
}

void TestSystemMixer::pactl_parseSink51ChannelOrder()
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(kPactlSinks51));
    QVERIFY(doc.isArray());

    const SinkState sink = PactlMixer::parseSink(doc.array().first().toObject());
    QCOMPARE(sink.ports.size(), 1);
    QCOMPARE(sink.ports.first().type, QStringLiteral("hdmi"));

    // Channel-map order (FL, FR, RL, RR, FC, LFE, SL, SR) — NOT alphabetical
    // (which would start FC, FL, FR, LFE and shuffle every set-volume call).
    const QVector<int> expected{65536, 32768, 49152, 16384, 60000, 1000, 2000, 3000};
    QCOMPARE(sink.volumes, expected);
}

void TestSystemMixer::pactl_parseSinkInput()
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(kPactlSinkInputs));
    QVERIFY(doc.isArray());
    const QJsonObject object = doc.array().first().toObject();

    const SinkInputInfo input = PactlMixer::parseSinkInput(object);
    QCOMPARE(input.index, 156);
    QCOMPARE(input.sinkIndex, 50);
    QVERIFY(!input.corked);
    QVERIFY(!input.mute);
    QCOMPARE(input.applicationName, QStringLiteral("pw-play"));
    QCOMPARE(input.nodeName, QStringLiteral("pw-play"));
    QCOMPARE(input.processId, qint64(-1)); // absent in this capture
    QCOMPARE(input.volumes, QVector<int>({65536, 65536}));

    // With application.process.id present (typical for real streams).
    QJsonObject withPid = object;
    QJsonObject properties = object.value(QStringLiteral("properties")).toObject();
    properties.insert(QStringLiteral("application.process.id"), QStringLiteral("1766"));
    withPid.insert(QStringLiteral("properties"), properties);
    QCOMPARE(PactlMixer::parseSinkInput(withPid).processId, qint64(1766));
}

void TestSystemMixer::pactl_parseInfoJsonAndText()
{
    const QString expected = QStringLiteral("alsa_output.pci-0000_00_1b.0.analog-stereo");
    QCOMPARE(PactlMixer::parseInfoDefaultSink(QByteArray(kPactlInfoJson)), expected);
    QCOMPARE(PactlMixer::parseInfoDefaultSink(QByteArray(kPactlInfoText)), expected);
    // Defensive: garbage yields empty, never a crash.
    QVERIFY(PactlMixer::parseInfoDefaultSink(QByteArrayLiteral("not info at all")).isEmpty());
}

void TestSystemMixer::pactl_portType()
{
    QCOMPARE(PactlMixer::portType(QStringLiteral("Speaker"), QJsonObject()),
             QStringLiteral("speaker"));
    QCOMPARE(PactlMixer::portType(QStringLiteral("Headphones"), QJsonObject()),
             QStringLiteral("headphones"));
    QCOMPARE(PactlMixer::portType(QStringLiteral("HDMI"), QJsonObject()),
             QStringLiteral("hdmi"));
    QCOMPARE(PactlMixer::portType(QStringLiteral("Line Out"), QJsonObject()),
             QStringLiteral("speaker"));

    QJsonObject bluetooth;
    bluetooth.insert(QStringLiteral("device.bus"), QStringLiteral("bluetooth"));
    QCOMPARE(PactlMixer::portType(QStringLiteral("unknown"), bluetooth),
             QStringLiteral("bluetooth"));

    QJsonObject formFactor;
    formFactor.insert(QStringLiteral("device.form_factor"), QStringLiteral("speaker"));
    QCOMPARE(PactlMixer::portType(QStringLiteral("unknown"), formFactor),
             QStringLiteral("speaker"));

    QCOMPARE(PactlMixer::portType(QString(), QJsonObject()), QStringLiteral("unknown"));
}

// ---------------------------------------------------------------------------
// WpctlMixer parsers
// ---------------------------------------------------------------------------

void TestSystemMixer::wpctl_parseStatus()
{
    const QVector<WpctlMixer::WpSink> sinks =
        WpctlMixer::parseStatus(QString::fromUtf8(kWpctlStatus));
    QCOMPARE(sinks.size(), 1);
    QCOMPARE(sinks.first().id, 50);
    QCOMPARE(sinks.first().description, QStringLiteral("Built-in Audio Analog Stereo"));
    QCOMPARE(sinks.first().volume, 0.61f);
    QVERIFY(!sinks.first().mute);
    QVERIFY(sinks.first().isDefault);
}

void TestSystemMixer::wpctl_parseStatusMutedAndSectionScoping()
{
    QString muted = QString::fromUtf8(kWpctlStatus);
    muted.replace(QStringLiteral("[vol: 0.61]"), QStringLiteral("[vol: 0.61 MUTED]"));

    const QVector<WpctlMixer::WpSink> sinks = WpctlMixer::parseStatus(muted);
    QCOMPARE(sinks.size(), 1); // muted source + video source stay ignored
    QVERIFY(sinks.first().mute);

    // get-volume line with and without the mute marker.
    bool mutedFlag = false;
    QCOMPARE(WpctlMixer::parseVolumeLine(QStringLiteral("Volume: 0.61"), &mutedFlag), 0.61f);
    QVERIFY(!mutedFlag);
    QCOMPARE(WpctlMixer::parseVolumeLine(QStringLiteral("Volume: 0.50 [MUTED]"), &mutedFlag),
             0.50f);
    QVERIFY(mutedFlag);
    QVERIFY(WpctlMixer::parseVolumeLine(QStringLiteral("garbage"), &mutedFlag) < 0.0f);
}

void TestSystemMixer::wpctl_parseInspect()
{
    const QString text = QString::fromUtf8(kWpctlInspect);
    QCOMPARE(WpctlMixer::parseNodeName(text),
             QStringLiteral("alsa_output.pci-0000_00_1b.0.analog-stereo"));
    QCOMPARE(WpctlMixer::parseAudioChannels(text), 2);
    QVERIFY(WpctlMixer::parseNodeName(QStringLiteral("no node here")).isEmpty());
    QCOMPARE(WpctlMixer::parseAudioChannels(QStringLiteral("no channels here")), 0);
}

void TestSystemMixer::wpctl_moveWithoutResolvedIdFails()
{
    // Routing needs the node.name -> id cache that listSinks() fills; with
    // a cold cache no process is spawned and the request fails politely.
    WpctlMixer wpctl;
    QSignalSpy moveFailed(&wpctl, &SystemMixer::moveError);

    wpctl.moveSinkInput(7, QStringLiteral("alsa_output.pci-0000_00_1b.0.hdmi"));
    QCOMPARE(moveFailed.count(), 1);
    QVERIFY(!moveFailed.first().at(0).toString().isEmpty());

    wpctl.moveSinkInput(7, QString()); // empty target rejected outright
    QCOMPARE(moveFailed.count(), 2);
}

// ---------------------------------------------------------------------------
// AmixerMixer parser
// ---------------------------------------------------------------------------

void TestSystemMixer::amixer_parseStereoMonoAndGarbage()
{
    // Stereo, channels disagree on mute: reported as NOT muted (muted only
    // when every channel is off, which is how we set it).
    const QString stereo =
        QStringLiteral("Simple mixer control 'Master',0\n"
                       "  Capabilities: pvolume pswitch\n"
                       "  Playback channels: Front Left - Front Right\n"
                       "  Limits: Playback 0 - 87\n"
                       "  Front Left: Playback 43 [50%] [-18.00dB] [on]\n"
                       "  Front Right: Playback 43 [50%] [-18.00dB] [off]");
    QCOMPARE(AmixerMixer::parseAmixerOutput(stereo).first, 50);
    QVERIFY(!AmixerMixer::parseAmixerOutput(stereo).second);

    // Mono, fully muted.
    const QString mono =
        QStringLiteral("Simple mixer control 'Master',0\n"
                       "  Mono: Playback 17 [20%] [-42.00dB] [off]");
    QCOMPARE(AmixerMixer::parseAmixerOutput(mono).first, 20);
    QVERIFY(AmixerMixer::parseAmixerOutput(mono).second);

    // Defensive: unparsable output never crashes, falls back to 100 %/unmuted.
    const auto fallback = AmixerMixer::parseAmixerOutput(QStringLiteral("nothing here"));
    QCOMPARE(fallback.first, 100);
    QVERIFY(!fallback.second);
}

// ---------------------------------------------------------------------------
// Fallback & factory
// ---------------------------------------------------------------------------

void TestSystemMixer::nullMixer_degradesGracefully()
{
    NullMixer mixer;

    QSignalSpy detected(&mixer, &SystemMixer::backendDetected);
    mixer.detect();
    QCOMPARE(detected.count(), 1);
    QCOMPARE(detected.first().at(0).toString(), QStringLiteral("null"));
    const MixerCapabilities caps = detected.first().at(1).value<MixerCapabilities>();
    QVERIFY(!caps.canSetVolume);
    QVERIFY(!caps.canMute);
    QVERIFY(!caps.hasSinkInputs);

    QSignalSpy sinkError(&mixer, &SystemMixer::sinkError);
    QSignalSpy inputsError(&mixer, &SystemMixer::sinkInputsError);
    QSignalSpy volumeError(&mixer, &SystemMixer::volumeError);

    mixer.getDefaultSink();
    QCOMPARE(sinkError.count(), 1);
    mixer.listSinkInputs();
    QCOMPARE(inputsError.count(), 1);
    mixer.setSinkVolumes(QStringLiteral("x"), {100});
    QCOMPARE(volumeError.count(), 1);

    // Watching is a no-op, not a crash.
    mixer.startWatch();
    mixer.stopWatch();
}

void TestSystemMixer::backendsWithoutEnumerationReportUnavailable()
{
    // Neither amixer nor the Null backend can enumerate devices or route
    // streams; both must answer with an error signal (never a crash, never
    // a silent success that would leave the UI offering dead devices).
    NullMixer nullMixer;
    QSignalSpy nullListed(&nullMixer, &SystemMixer::sinksError);
    QSignalSpy nullMove(&nullMixer, &SystemMixer::moveError);
    nullMixer.listSinks();
    nullMixer.moveSinkInput(7, QStringLiteral("x"));
    QCOMPARE(nullListed.count(), 1);
    QCOMPARE(nullMove.count(), 1);

    AmixerMixer amixer;
    QSignalSpy amixerListed(&amixer, &SystemMixer::sinksError);
    QSignalSpy amixerMove(&amixer, &SystemMixer::moveError);
    amixer.listSinks();
    amixer.moveSinkInput(7, QStringLiteral("x"));
    QCOMPARE(amixerListed.count(), 1);
    QCOMPARE(amixerMove.count(), 1);
}

void TestSystemMixer::factory_picksBackend()
{
    QVERIFY(qobject_cast<PactlMixer *>(
        SystemMixer::createBackend(QStringLiteral("pactl"))));
    QVERIFY(qobject_cast<WpctlMixer *>(
        SystemMixer::createBackend(QStringLiteral("wpctl"))));
    QVERIFY(qobject_cast<AmixerMixer *>(
        SystemMixer::createBackend(QStringLiteral("amixer"))));
    QVERIFY(qobject_cast<NullMixer *>(
        SystemMixer::createBackend(QStringLiteral("bogus"))));

    // Presence-based pick must at least return a working object whose type
    // matches whichever tool is installed on this machine.
    std::unique_ptr<SystemMixer> autoPicked(SystemMixer::create());
    QVERIFY(autoPicked != nullptr);
    if (!QStandardPaths::findExecutable(QStringLiteral("pactl")).isEmpty())
        QVERIFY(qobject_cast<PactlMixer *>(autoPicked.get()) != nullptr);
    else if (!QStandardPaths::findExecutable(QStringLiteral("wpctl")).isEmpty())
        QVERIFY(qobject_cast<WpctlMixer *>(autoPicked.get()) != nullptr);
    else
        QVERIFY(qobject_cast<NullMixer *>(autoPicked.get()) != nullptr);
}

QTEST_GUILESS_MAIN(TestSystemMixer)
#include "tst_systemmixer.moc"
