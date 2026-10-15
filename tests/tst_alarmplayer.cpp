#include "audio/AlarmPlayer.h"
#include "audio/FakeMixer.h"

#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

// The ring player without any real audio device: all media access goes
// through the startMedia()/stopMedia() seam, so what is tested here is the
// contract the controller relies on - path validation, the stream-volume
// clamp (never > 1.0) and the mandatory stream verification/forcing against
// FakeMixer (the stream-restore hazard countermeasure).
class TestAlarmPlayer : public QObject
{
    Q_OBJECT

private slots:
    void constructionHasExpectedDefaults();
    void streamVolumeIsClampedAndNeverAboveOne();
    void missingFileReportsErrorAndDoesNotPlay();
    void playStartsLoopingPlayback();
    void previewPlaysOnce();
    void noMixerPlaysWithoutVerification();
    void verificationAcceptsMatchingStream();
    void verificationForcesWrongVolume();
    void verificationUnmutesOurStream();
    void verificationMatchesByNameWhenPidIsUnknown();
    void verificationIgnoresForeignStreams();
    void verificationGivesUpAfterAttempts();
    void streamErrorStopsVerification();
    void stopCancelsVerification();
    void previewDoesNotVerify();
    void outputSinkRoutesOurStreamOnceLocated();
    void emptyOutputSinkNeverMoves();
    void moveFailureKeepsVerificationIntact();

private:
    static QString existingFile(QTemporaryDir &dir, const QString &name);
};

QString TestAlarmPlayer::existingFile(QTemporaryDir &dir,
                                      const QString &name)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("not a real tone - media is faked in these tests");
        file.close();
    }
    return path;
}

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

namespace {

SinkInputInfo ourStream(const QVector<int> &volumes, bool mute = false)
{
    SinkInputInfo input;
    input.index = 7;
    input.sinkIndex = 50;
    input.applicationName = QStringLiteral("some-other-app");
    input.processId = 999999; // overwritten by tests that need "us"
    input.volumes = volumes;
    input.mute = mute;
    return input;
}

} // namespace

void TestAlarmPlayer::constructionHasExpectedDefaults()
{
    FakePlayer player;

    QCOMPARE(player.streamVolume(), 100); // full stream, sink floor decides
    QCOMPARE(player.outputVolume(), 1.0f);
    QVERIFY(!player.isPlaying());
}

void TestAlarmPlayer::streamVolumeIsClampedAndNeverAboveOne()
{
    FakePlayer player;

    player.setStreamVolume(40);
    QCOMPARE(player.streamVolume(), 40);
    QCOMPARE(player.outputVolume(), 0.4f);

    // The guide's hard rule: no software amplification, ever.
    player.setStreamVolume(250);
    QCOMPARE(player.streamVolume(), 100);
    QCOMPARE(player.outputVolume(), 1.0f);
    QVERIFY(player.outputVolume() <= 1.0f);

    player.setStreamVolume(-5);
    QCOMPARE(player.streamVolume(), 0);
    QVERIFY(player.outputVolume() == 0.0f);
}

void TestAlarmPlayer::missingFileReportsErrorAndDoesNotPlay()
{
    FakePlayer player;
    QSignalSpy errors(&player, &AlarmPlayer::playbackError);

    player.play(QStringLiteral("/nonexistent/lumalarm-test-tone.wav"));
    QCOMPARE(errors.count(), 1);
    QVERIFY(errors.at(0).at(0).toString().contains(
        QStringLiteral("not found")));
    QVERIFY(!player.isPlaying());

    player.preview(QString());
    QCOMPARE(errors.count(), 2);
    QVERIFY(!player.isPlaying());

    player.stop(); // stays safe afterwards
}

void TestAlarmPlayer::playStartsLoopingPlayback()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakePlayer player;
    QSignalSpy playingSpy(&player, &AlarmPlayer::isPlayingChanged);

    player.play(file);
    QCOMPARE(player.startCount, 1);
    QCOMPARE(player.lastPath, file);
    QVERIFY(player.lastLooping);
    QVERIFY(player.isPlaying());
    QCOMPARE(playingSpy.count(), 1);
    QCOMPARE(player.stopCount, 1); // play() stopped the idle player first

    player.stop();
    QCOMPARE(player.stopCount, 2);
    QVERIFY(!player.isPlaying());
    QCOMPARE(playingSpy.count(), 2);
}

void TestAlarmPlayer::previewPlaysOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakePlayer player;
    player.preview(file);
    QCOMPARE(player.startCount, 1);
    QVERIFY(!player.lastLooping);
    QVERIFY(player.isPlaying());
}

void TestAlarmPlayer::noMixerPlaysWithoutVerification()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakePlayer player;
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);

    player.play(file);
    QVERIFY(player.isPlaying());
    // No mixer: never any stream query, and no verification signal at all.
    QCOMPARE(verified.count(), 0);
    QCOMPARE(player.startCount, 1);
}

void TestAlarmPlayer::verificationAcceptsMatchingStream()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({65536, 65536});
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy volumeForced(&mixer, &SystemMixer::sinkInputVolumeApplied);

    player.play(file);

    QCOMPARE(verified.count(), 1);
    QVERIFY(verified.at(0).at(0).toBool());
    // Already correct: nothing may be forced (stream-restore must not be
    // "fixed" into a change).
    QCOMPARE(volumeForced.count(), 0);
}

void TestAlarmPlayer::verificationForcesWrongVolume()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({13107, 13107}); // 20% - the hazard
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer);
    player.setStreamVolume(100);
    player.setVerifyIntervalMs(5);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);

    player.play(file);

    // First try forces the requested level, the next one confirms it.
    QTRY_COMPARE(verified.count(), 1);
    QVERIFY(verified.at(0).at(0).toBool());
    QCOMPARE(mixer.lastSinkInputIndex(), 7);
    QCOMPARE(mixer.lastSinkInputVolumes(),
             QVector<int>({65536, 65536}));
}

void TestAlarmPlayer::verificationUnmutesOurStream()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({65536, 65536}, /*mute=*/true);
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer);
    player.setVerifyIntervalMs(5);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy muteForced(&mixer, &SystemMixer::sinkInputMuteApplied);

    player.play(file);

    QTRY_COMPARE(verified.count(), 1);
    QVERIFY(verified.at(0).at(0).toBool());
    QCOMPARE(muteForced.count(), 1);
    QCOMPARE(muteForced.at(0).at(0).toInt(), 7);
}

void TestAlarmPlayer::verificationMatchesByNameWhenPidIsUnknown()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    // wpctl gives no process id: the application name is the fallback.
    FakeMixer mixer;
    SinkInputInfo input = ourStream({65536, 65536});
    input.processId = -1;
    input.applicationName = QCoreApplication::applicationName();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);

    player.play(file);

    QCOMPARE(verified.count(), 1);
    QVERIFY(verified.at(0).at(0).toBool());
}

void TestAlarmPlayer::verificationIgnoresForeignStreams()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    // Someone else's stream at 100%: touching it would be a bug.
    FakeMixer mixer;
    mixer.addSinkInput(ourStream({65536, 65536}));

    FakePlayer player;
    player.setMixer(&mixer);
    player.setVerifyAttempts(2);
    player.setVerifyIntervalMs(5);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy volumeForced(&mixer, &SystemMixer::sinkInputVolumeApplied);

    player.play(file);

    QTRY_COMPARE(verified.count(), 1);
    QVERIFY(!verified.at(0).at(0).toBool());
    QCOMPARE(volumeForced.count(), 0);
}

void TestAlarmPlayer::verificationGivesUpAfterAttempts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer; // no sink-input at all: the stream never showed up

    FakePlayer player;
    player.setMixer(&mixer);
    player.setVerifyAttempts(3);
    player.setVerifyIntervalMs(5);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy listed(&mixer, &SystemMixer::sinkInputsReady);

    player.play(file);

    QTRY_COMPARE(verified.count(), 1);
    QVERIFY(!verified.at(0).at(0).toBool());
    QCOMPARE(listed.count(), 3); // exactly the configured window
}

void TestAlarmPlayer::streamErrorStopsVerification()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    class ErrorMixer : public FakeMixer
    {
    public:
        using FakeMixer::FakeMixer;
        void listSinkInputs() override
        {
            emit sinkInputsError(QStringLiteral("no stream control"));
        }
    };
    ErrorMixer mixer;

    FakePlayer player;
    player.setMixer(&mixer);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);

    player.play(file);

    QCOMPARE(verified.count(), 1);
    QVERIFY(!verified.at(0).at(0).toBool());
}

void TestAlarmPlayer::stopCancelsVerification()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;

    FakePlayer player;
    player.setMixer(&mixer);
    player.setVerifyAttempts(50);
    player.setVerifyIntervalMs(5);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy listed(&mixer, &SystemMixer::sinkInputsReady);

    player.play(file);
    player.stop(); // user dismissed the ring mid-verification

    const int listedAtStop = listed.count();
    QTest::qWait(50);
    QCOMPARE(listed.count(), listedAtStop); // no further queries
    QCOMPARE(verified.count(), 0); // silent cancel, no misleading result
}

void TestAlarmPlayer::previewDoesNotVerify()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({13107, 13107});
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer);
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy listed(&mixer, &SystemMixer::sinkInputsReady);

    player.preview(file);

    QTest::qWait(20);
    QCOMPARE(listed.count(), 0);
    QCOMPARE(verified.count(), 0);
}

void TestAlarmPlayer::outputSinkRoutesOurStreamOnceLocated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({65536, 65536});
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer);
    player.setOutputSink(QStringLiteral("fake-hdmi-output"));
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);

    player.play(file);

    // Exactly one move, for OUR stream, to the configured device — even
    // though the volume was already correct (routing is not optional).
    QCOMPARE(verified.count(), 1);
    QCOMPARE(mixer.moveCallCount(), 1);
    QCOMPARE(mixer.lastMovedIndex(), input.index);
    QCOMPARE(mixer.lastMovedSink(), QStringLiteral("fake-hdmi-output"));

    player.stop();
}

void TestAlarmPlayer::emptyOutputSinkNeverMoves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({65536, 65536});
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);

    FakePlayer player;
    player.setMixer(&mixer); // default outputSink = "" (follow system default)

    player.play(file);

    QCOMPARE(mixer.moveCallCount(), 0);
    player.stop();
}

void TestAlarmPlayer::moveFailureKeepsVerificationIntact()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = existingFile(dir, QStringLiteral("beep.wav"));

    FakeMixer mixer;
    SinkInputInfo input = ourStream({65536, 65536});
    input.processId = QCoreApplication::applicationPid();
    mixer.addSinkInput(input);
    mixer.setNextMoveResult(false);

    FakePlayer player;
    player.setMixer(&mixer);
    player.setOutputSink(QStringLiteral("unplugged-device"));
    QSignalSpy verified(&player, &AlarmPlayer::streamVerified);
    QSignalSpy moveFailed(&mixer, &SystemMixer::moveError);

    player.play(file);

    // A failed route is logged, never fatal: the ring verifies and keeps
    // playing on the default device.
    QCOMPARE(moveFailed.count(), 1);
    QCOMPARE(verified.count(), 1);
    QVERIFY(verified.at(0).at(0).toBool());
    QVERIFY(player.isPlaying());

    player.stop();
}

QTEST_GUILESS_MAIN(TestAlarmPlayer)

#include "tst_alarmplayer.moc"
