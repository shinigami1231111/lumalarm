#include <QSignalSpy>
#include <QtTest>

#include "audio/AudioPlayer.h"

// ============================================================================
// Unit tests for AudioPlayer (src/audio/). Covers the pure state/property
// surface that needs no audio device: construction defaults, the documented
// clamping of volume/baseVolume/fadeDuration, and the missing-file error path
// of play() which must report through playbackError instead of touching the
// multimedia backend.
//
// Not covered (needs a real output device): actual fade-in curves and
// playback state transitions. Follow-up: refactor for an injectable audio
// backend to make those testable.
// ============================================================================

class TestAudioPlayer : public QObject
{
    Q_OBJECT

private slots:
    void constructionHasExpectedDefaults();
    void volumeRoundTripsAndIsClamped();
    void baseVolumeAndFadeDurationAreClamped();
    void playMissingFileReportsErrorAndDoesNotPlay();
};

void TestAudioPlayer::constructionHasExpectedDefaults()
{
    AudioPlayer player;

    QCOMPARE(player.volume(), 80);       // documented default
    QCOMPARE(player.baseVolume(), 20);   // alarm fade floor
    QCOMPARE(player.fadeDuration(), 15); // seconds
    QCOMPARE(player.isPlaying(), false);
}

void TestAudioPlayer::volumeRoundTripsAndIsClamped()
{
    AudioPlayer player;
    QSignalSpy spy(&player, &AudioPlayer::volumeChanged);

    player.setVolume(55);
    QCOMPARE(player.volume(), 55);
    QCOMPARE(spy.count(), 1);

    // Setting the same value again must not re-emit.
    player.setVolume(55);
    QCOMPARE(spy.count(), 1);

    // Out-of-range values are clamped to 0..100.
    player.setVolume(500);
    QCOMPARE(player.volume(), 100);
    player.setVolume(-10);
    QCOMPARE(player.volume(), 0);
}

void TestAudioPlayer::baseVolumeAndFadeDurationAreClamped()
{
    AudioPlayer player;

    player.setBaseVolume(35);
    QCOMPARE(player.baseVolume(), 35);
    player.setBaseVolume(50); // upper bound
    QCOMPARE(player.baseVolume(), 50);
    player.setBaseVolume(999);
    QCOMPARE(player.baseVolume(), 50);

    player.setFadeDuration(30);
    QCOMPARE(player.fadeDuration(), 30);
    player.setFadeDuration(1); // lower bound is 5s
    QCOMPARE(player.fadeDuration(), 5);
    player.setFadeDuration(9999); // upper bound is 60s
    QCOMPARE(player.fadeDuration(), 60);
}

void TestAudioPlayer::playMissingFileReportsErrorAndDoesNotPlay()
{
    AudioPlayer player;
    QSignalSpy errSpy(&player, &AudioPlayer::playbackError);

    // resolvePath() returns empty for a non-existent file, so play() must bail
    // out before touching QMediaPlayer.
    player.play(QStringLiteral("/nonexistent/lumalarm-test-tone.wav"));

    QCOMPARE(errSpy.count(), 1);
    QVERIFY(errSpy.at(0).at(0).toString().contains(QStringLiteral("not found")));
    QCOMPARE(player.isPlaying(), false);

    // Empty path is treated the same way, and stop() stays safe afterwards.
    player.play(QString());
    QCOMPARE(errSpy.count(), 2);
    player.stop();
    QCOMPARE(player.isPlaying(), false);
}

QTEST_GUILESS_MAIN(TestAudioPlayer)
#include "tst_audioplayer.moc"
