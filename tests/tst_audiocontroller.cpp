#include "audio/AlarmAudioController.h"
#include "audio/FakeMixer.h"
#include "audio/MprisPause.h"
#include "audio/RestoreJournal.h"
#include "audio/SoundResolver.h"
#include "audio/VolumePolicy.h"

#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

// The ring flow end to end, without any real audio device or CLI backend:
// FakeMixer answers synchronously, the journal lives in a QTemporaryDir, the
// player is a media-seam subclass, and the MPRIS hook is a counting no-op —
// so no test run can ever pause a real player on this machine.
//
// What is asserted here is the guide's ordering contract, layer by layer:
//   * the journal is written BEFORE the sink moves (and a failed write means
//     the sink is never touched);
//   * stop restores the sink exactly and clears the journal;
//   * alarmOnly never touches the sink at all;
//   * a user change beyond the 3 % tolerance stops the managing for good;
//   * the quit path restores synchronously within its bound;
//   * start() recovers what a crashed previous run left behind.
class TestAudioController : public QObject
{
    Q_OBJECT

private slots:
    void ringGuaranteeJournalsBeforeRaise();
    void ringAlarmOnlyNeverTouchesSink();
    void ringWithoutSoundIsSilentAndDoesNoSinkWork();
    void defaultSoundUsedWhenPerAlarmSoundIsEmpty();
    void stopDelegatesToRingStopAndRestoresExactly();
    void userOverrideWithinToleranceKeepsManaging();
    void userOverrideBeyondToleranceMeansNoRestore();
    void journalWriteFailureLeavesSinkAloneButStillPlays();
    void quitDuringRingRestoresSynchronouslyBounded();
    void startRecoversLeftoverJournal();
    void qmlSurfacePlayPreviewStop();
    void unknownNameReportsErrorAndPlaysNothing();
    void fallbackTonePreviewWorksWithAlarmVolumePolicy();
    void refreshOutputDevicesListsNamedSinksAndWarmsRouting();
};

// Media seam: records calls instead of touching QMediaPlayer.
class FakePlayer : public AlarmPlayer
{
public:
    using AlarmPlayer::AlarmPlayer;

    QString lastPath;
    bool lastLooping = false;
    int startCount = 0;
    int stopCount = 0;

protected:
    void startMedia(const QString &filePath, bool looping) override
    {
        lastPath = filePath;
        lastLooping = looping;
        ++startCount;
    }
    void stopMedia() override { ++stopCount; }
};

// MPRIS seam: counts, never touches the bus.
class FakeMpris : public MprisPause
{
public:
    int pauseCount = 0;
    int resumeCount = 0;

    void pauseAll() override { ++pauseCount; }
    void resumeAll() override { ++resumeCount; }
};

namespace {

SinkState makeSink(const QVector<int> &volumes, bool mute = false)
{
    SinkState sink;
    sink.name = QStringLiteral("fake-sink");
    sink.volumes = volumes;
    sink.mute = mute;
    return sink;
}

} // namespace

// Everything a test needs, in dependency order (members initialise top down).
class Fixture
{
public:
    Fixture()
        : journal(state.filePath(QStringLiteral("audio-restore.json")))
        , resolver(QStringList{tones.path()})
        , controller(&mixer, &player, &journal, resolver, &mpris)
    {
        // Short verification window: no sink-inputs exist in these tests, so
        // verification always runs out — quickly.
        player.setVerifyAttempts(2);
        player.setVerifyIntervalMs(5);
    }

    QString writeTone(const QString &name)
    {
        const QString path = tones.filePath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("not a real tone - media is faked in these tests");
            file.close();
        }
        return path;
    }

    AlarmAudioController::Settings
    settings(VolumePolicy::Kind policy = VolumePolicy::Kind::Guarantee,
             int floorPercent = 60)
    {
        AlarmAudioController::Settings s;
        s.policy = policy;
        s.floorPercent = floorPercent;
        s.streamPercent = 100;
        return s;
    }

    QTemporaryDir tones;
    QTemporaryDir state;
    FakeMixer mixer;
    FakePlayer player;
    FakeMpris mpris;
    RestoreJournal journal;
    SoundResolver resolver;
    AlarmAudioController controller;
};

void TestAudioController::ringGuaranteeJournalsBeforeRaise()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings()); // guarantee, floor 60 %

    f.controller.ring(QStringLiteral("tone.wav"));

    // The stream rings (looped) and the MPRIS hook fired.
    QVERIFY(f.player.isPlaying());
    QVERIFY(f.player.lastLooping);
    QCOMPARE(f.mpris.pauseCount, 1);

    // The sink was raised exactly once, to the journaled state.
    QCOMPARE(f.mixer.applyVolumeCallCount(), 1);
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{39321, 39321}));

    // The journal holds ORIGINAL and APPLIED — write-ahead, both halves.
    const auto entry = f.journal.load();
    QVERIFY(entry.has_value());
    QCOMPARE(entry->sinkName, QStringLiteral("fake-sink"));
    QCOMPARE(entry->originalVolumes, (QVector<int>{32768, 32768}));
    QVERIFY(!entry->originalMute);
    QCOMPARE(entry->appliedVolumes, (QVector<int>{39321, 39321}));
    QVERIFY(!entry->appliedMute);

    // The applied state matches what actually landed on the sink.
    QCOMPARE(f.mixer.sink().volumes, entry->appliedVolumes);
}

void TestAudioController::ringAlarmOnlyNeverTouchesSink()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({20000, 20000}, /*mute=*/true));
    f.controller.applySettings(f.settings(VolumePolicy::Kind::AlarmOnly));

    f.controller.ring(QStringLiteral("tone.wav"));

    // The stream plays — but the sink is untouched and no journal exists.
    QVERIFY(f.player.isPlaying());
    QCOMPARE(f.mixer.applyVolumeCallCount(), 0);
    QCOMPARE(f.mixer.applyMuteCallCount(), 0);
    QVERIFY(!QFile::exists(f.journal.path()));
    // No sink read either: the policy never even asks.
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{20000, 20000}));
    QVERIFY(f.mixer.sink().mute);
}

void TestAudioController::ringWithoutSoundIsSilentAndDoesNoSinkWork()
{
    Fixture f; // no tone written, no default sound configured
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings());

    f.controller.ring(QString());

    // No audio, no sink work, no journal — but MPRIS is still paused and
    // stopRing() still resumes it (one ring lifecycle, regardless of sound).
    QCOMPARE(f.player.startCount, 0);
    QVERIFY(!f.player.isPlaying());
    QCOMPARE(f.mixer.applyVolumeCallCount(), 0);
    QVERIFY(!QFile::exists(f.journal.path()));
    QCOMPARE(f.mpris.pauseCount, 1);

    f.controller.stopRing();
    QCOMPARE(f.mpris.resumeCount, 1);
}

void TestAudioController::defaultSoundUsedWhenPerAlarmSoundIsEmpty()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({10000, 10000}));
    auto s = f.settings();
    s.defaultSound = QStringLiteral("tone.wav");
    f.controller.applySettings(s);

    f.controller.ring(QString()); // per-alarm empty -> global default

    QVERIFY(f.player.isPlaying());
    QVERIFY(f.player.lastPath.endsWith(QStringLiteral("tone.wav")));
    QCOMPARE(f.mixer.applyVolumeCallCount(), 1);
    QVERIFY(QFile::exists(f.journal.path()));
}

void TestAudioController::stopDelegatesToRingStopAndRestoresExactly()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({32768, 32768}, /*mute=*/true));
    f.controller.applySettings(f.settings());

    f.controller.ring(QStringLiteral("tone.wav"));
    QCOMPARE(f.mixer.applyVolumeCallCount(), 1);

    // A sink event within the tolerance is NOT the user taking over.
    f.mixer.simulateSinkChange(makeSink({39321 - 1000, 39321 - 1000}));
    QVERIFY(QFile::exists(f.journal.path()));

    // QML stop() during a ring must run the full ring stop (overlay path).
    f.controller.stop();

    QVERIFY(!f.player.isPlaying());
    QCOMPARE(f.mpris.resumeCount, 1);
    // Restored exactly to the journaled original — including the mute.
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{32768, 32768}));
    QVERIFY(f.mixer.sink().mute);
    // raise + restore volume, restore mute
    QCOMPARE(f.mixer.applyVolumeCallCount(), 2);
    QCOMPARE(f.mixer.applyMuteCallCount(), 2);
    QVERIFY(!QFile::exists(f.journal.path()));
}

void TestAudioController::userOverrideWithinToleranceKeepsManaging()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings());

    f.controller.ring(QStringLiteral("tone.wav"));
    QVERIFY(QFile::exists(f.journal.path()));

    // 1000 raw < 1966 tolerance: still ours, journal stays.
    f.mixer.simulateSinkChange(makeSink({39321 - 1000, 39321 - 1000}));
    QVERIFY(QFile::exists(f.journal.path()));

    // A stop afterwards still restores the original.
    f.controller.stopRing();
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{32768, 32768}));
    QVERIFY(!QFile::exists(f.journal.path()));
}

void TestAudioController::userOverrideBeyondToleranceMeansNoRestore()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings());

    f.controller.ring(QStringLiteral("tone.wav"));
    QCOMPARE(f.mixer.applyVolumeCallCount(), 1);
    QVERIFY(QFile::exists(f.journal.path()));

    // The user drags the sink far away mid-ring: journal dropped at once.
    f.mixer.simulateSinkChange(makeSink({1000, 1000}));
    QVERIFY(!QFile::exists(f.journal.path()));

    // Stopping must NOT fight back: no restore, no further sink writes.
    f.controller.stopRing();
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{1000, 1000}));
    QCOMPARE(f.mixer.applyVolumeCallCount(), 1); // only the raise ever ran
    QVERIFY(!f.player.isPlaying());
    QCOMPARE(f.mpris.resumeCount, 1);
}

void TestAudioController::journalWriteFailureLeavesSinkAloneButStillPlays()
{
    QTemporaryDir tones;
    QTemporaryDir state;
    // A FILE where the parent directory belongs: mkpath fails, so record()
    // must fail too. (A merely *missing* directory is no longer impossible —
    // Phase 6: record() creates it, otherwise a fresh machine never raises.)
    const QString blocker = state.filePath(QStringLiteral("blocker"));
    {
        QFile blockerFile(blocker);
        QVERIFY(blockerFile.open(QIODevice::WriteOnly));
        blockerFile.write("x");
    }
    FakeMixer mixer;
    FakePlayer player;
    player.setVerifyAttempts(2);
    player.setVerifyIntervalMs(5);
    FakeMpris mpris;
    RestoreJournal journal(blocker + QStringLiteral("/journal.json"));
    SoundResolver resolver(QStringList{tones.path()});
    AlarmAudioController controller(&mixer, &player, &journal, resolver, &mpris);
    // A complete entry against an impossible path: record() must fail on the
    // atomic write, not on validation.
    RestoreJournal::Entry validEntry;
    validEntry.sinkName = QStringLiteral("fake-sink");
    validEntry.originalVolumes = {32768, 32768};
    validEntry.appliedVolumes = {39321, 39321};
    QVERIFY(!journal.record(validEntry));

    QFile tone(tones.filePath(QStringLiteral("tone.wav")));
    QVERIFY(tone.open(QIODevice::WriteOnly));
    tone.write("fake");
    tone.close();

    mixer.setSink(makeSink({32768, 32768}));
    controller.applySettings(
        AlarmAudioController::Settings{}); // guarantee, floor 50
    controller.ring(QStringLiteral("tone.wav"));

    // Crash-safety contract: no journal => the sink is NEVER touched, but
    // the alarm still rings (at the stream volume, sink as the user left it).
    QCOMPARE(mixer.applyVolumeCallCount(), 0);
    QCOMPARE(mixer.sink().volumes, (QVector<int>{32768, 32768}));
    QVERIFY(player.isPlaying());
    QVERIFY(!QFile::exists(journal.path()));
}

void TestAudioController::quitDuringRingRestoresSynchronouslyBounded()
{
    Fixture f;
    f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings());

    f.controller.ring(QStringLiteral("tone.wav"));
    QVERIFY(QFile::exists(f.journal.path()));

    QElapsedTimer timer;
    timer.start();
    f.controller.shutdownForQuit();
    const qint64 elapsed = timer.elapsed();

    // Guide: synchronous before teardown, bounded well under a second.
    QVERIFY(elapsed < 1000);
    QVERIFY(!f.player.isPlaying());
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{32768, 32768}));
    QVERIFY(!QFile::exists(f.journal.path()));
    QCOMPARE(f.mpris.resumeCount, 1);

    // Idempotent: a second quit call writes nothing more.
    const int volumeWrites = f.mixer.applyVolumeCallCount();
    f.controller.shutdownForQuit();
    QCOMPARE(f.mixer.applyVolumeCallCount(), volumeWrites);
}

void TestAudioController::startRecoversLeftoverJournal()
{
    Fixture f;
    // What a crashed previous run left: sink still at what it applied.
    RestoreJournal::Entry entry;
    entry.sinkName = QStringLiteral("fake-sink");
    entry.originalVolumes = {52429, 52429};
    entry.originalMute = false;
    entry.appliedVolumes = {65536, 65536};
    entry.appliedMute = false;
    QVERIFY(f.journal.record(entry));
    f.mixer.setSink(makeSink({65536, 65536}));

    f.controller.start(); // probe + recover

    // Compare-and-set matched -> original written back exactly, journal gone.
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{52429, 52429}));
    QVERIFY(!QFile::exists(f.journal.path()));
    QCOMPARE(f.player.startCount, 0); // recovery is not playback
}

void TestAudioController::qmlSurfacePlayPreviewStop()
{
    Fixture f;
    const QString tone = f.writeTone(QStringLiteral("tone.wav"));
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings());

    QSignalSpy errors(&f.controller, &AlarmAudioController::playbackError);

    // play(): looped, NO sink policy (a timer is not an alarm ring).
    f.controller.play(QStringLiteral("tone.wav"));
    QVERIFY(f.controller.isPlaying());
    QVERIFY(f.player.lastLooping);
    QCOMPARE(f.player.lastPath, tone);
    QCOMPARE(f.mixer.applyVolumeCallCount(), 0);
    QVERIFY(!QFile::exists(f.journal.path()));
    QCOMPARE(errors.count(), 0);

    // stop(): plain playback stop, MPRIS untouched outside a ring.
    f.controller.stop();
    QVERIFY(!f.controller.isPlaying());
    QCOMPARE(f.mpris.pauseCount, 0);
    QCOMPARE(f.mpris.resumeCount, 0);

    // preview(): one shot.
    f.controller.preview(QStringLiteral("tone.wav"));
    QVERIFY(f.controller.isPlaying());
    QVERIFY(!f.player.lastLooping);
}

void TestAudioController::unknownNameReportsErrorAndPlaysNothing()
{
    // No tones at all: the whole chain (per-alarm -> default -> first tone)
    // is dead, so play() must fail loudly and start nothing.
    Fixture f;
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings());

    QSignalSpy errors(&f.controller, &AlarmAudioController::playbackError);
    f.controller.play(QStringLiteral("does-not-exist.wav"));

    QVERIFY(!f.controller.isPlaying());
    QCOMPARE(f.player.startCount, 0);
    QCOMPARE(errors.count(), 1);
    QCOMPARE(f.mixer.applyVolumeCallCount(), 0);
}

void TestAudioController::fallbackTonePreviewWorksWithAlarmVolumePolicy()
{
    Fixture f;
    // A brand-new profile has no tone files at all.
    QVERIFY(f.resolver.resolve(QStringLiteral("tone.wav"), QString()).isEmpty());
    f.mixer.setSink(makeSink({32768, 32768}));
    f.controller.applySettings(f.settings()); // guarantee, floor 60 %

    QSignalSpy errors(&f.controller, &AlarmAudioController::playbackError);
    f.controller.previewDefaultToneWithAlarmVolume();

    // The generated WAV is a real PCM file in a temp folder (not the tones
    // dir), and playback goes through the ring path, so the sink is raised.
    QCOMPARE(errors.count(), 0);
    QCOMPARE(f.player.startCount, 1);
    QVERIFY(QFile::exists(f.player.lastPath));
    QVERIFY(!f.player.lastPath.startsWith(f.tones.path()));
    QFile wav(f.player.lastPath);
    QVERIFY(wav.open(QIODevice::ReadOnly));
    const QByteArray header = wav.read(12);
    QCOMPARE(header.mid(0, 4), QByteArray("RIFF"));
    QCOMPARE(header.mid(8, 4), QByteArray("WAVE"));
    QVERIFY(wav.size() > 44);
    QCOMPARE(f.mixer.applyVolumeCallCount(), 1);
    QVERIFY(f.controller.isPlaying());

    f.controller.stopRing();
    QVERIFY(!f.controller.isPlaying());
    QCOMPARE(f.mixer.sink().volumes, (QVector<int>{32768, 32768}));
    QVERIFY(!QFile::exists(f.journal.path()));
}

void TestAudioController::refreshOutputDevicesListsNamedSinksAndWarmsRouting()
{
    Fixture f;

    SinkState speakers;
    speakers.index = 50;
    speakers.name = QStringLiteral("alsa_output.pci-0000_00_1b.0.analog-stereo");
    speakers.description = QStringLiteral("Built-in Audio Analog Stereo");
    SinkState hdmi;
    hdmi.index = 52;
    hdmi.name = QStringLiteral("alsa_output.pci-0000_00_1f.3.hdmi-stereo");
    hdmi.description = QStringLiteral("HDMI / DisplayPort 1");
    SinkState nameless; // no node.name: cannot be addressed -> not offered
    nameless.index = 53;
    f.mixer.setDevices({speakers, hdmi, nameless});

    QSignalSpy devicesChanged(&f.controller,
                              &AlarmAudioController::outputDevicesChanged);
    f.controller.refreshOutputDevices();

    QCOMPARE(devicesChanged.count(), 1);
    const QVariantList devices = f.controller.outputDevices();
    QCOMPARE(devices.size(), 2);
    QCOMPARE(devices.at(0).toMap().value(QStringLiteral("name")).toString(),
             speakers.name);
    QCOMPARE(devices.at(0).toMap().value(QStringLiteral("description")).toString(),
             speakers.description);
    QCOMPARE(devices.at(1).toMap().value(QStringLiteral("name")).toString(),
             hdmi.name);

    // Choosing a device warms the backend exactly once (wpctl resolves
    // ids lazily); unrelated configChanged() writes never spawn a CLI call.
    const int warmCalls = f.mixer.listSinksCallCount();
    AlarmAudioController::Settings s = f.settings();
    s.outputSink = speakers.name;
    f.controller.applySettings(s);
    QCOMPARE(f.mixer.listSinksCallCount(), warmCalls + 1);
    QCOMPARE(f.player.outputSink(), speakers.name);

    s.floorPercent = 80; // unrelated setting change
    f.controller.applySettings(s);
    QCOMPARE(f.mixer.listSinksCallCount(), warmCalls + 1);
}

QTEST_GUILESS_MAIN(TestAudioController)

#include "tst_audiocontroller.moc"
