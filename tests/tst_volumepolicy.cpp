#include "audio/VolumePolicy.h"

#include <QtTest>

using VolumePolicy::Kind;

// Pure decision logic: policy kind parsing, the guarantee floor raise
// (balance-preserving factor, hard 100 % ceiling), the 3 %-tolerance
// user-override comparison and the stream clamp. No mixer, no files.
class TestVolumePolicy : public QObject
{
    Q_OBJECT

private slots:
    void kindParsing_data();
    void kindParsing();
    void kindToStringRoundTrip();

    void alarmOnlyNeverTouches();
    void guaranteeSkipsWhenLoudEnough();
    void guaranteeUnmutesWithoutChangingVolume();
    void guaranteeRaisesBalancedSink();
    void guaranteePreservesBalance();
    void guaranteeClampsAtFullScale();
    void guaranteeLiftsSilentChannel();
    void guaranteeFloorZeroIsNoOp();
    void guaranteeHandlesEmptyState();

    void streamClampNeverExceedsFullScale();

    void matchToleranceBoundary();
    void userOverrideDetection_data();
    void userOverrideDetection();
};

void TestVolumePolicy::kindParsing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("expected");

    QTest::newRow("guarantee") << QStringLiteral("guarantee")
                               << int(Kind::Guarantee);
    QTest::newRow("guarantee mixed case") << QStringLiteral("Guarantee")
                                          << int(Kind::Guarantee);
    QTest::newRow("alarmOnly") << QStringLiteral("alarmOnly")
                               << int(Kind::AlarmOnly);
    QTest::newRow("alarmOnly upper") << QStringLiteral("ALARMONLY")
                                     << int(Kind::AlarmOnly);
    QTest::newRow("alarmOnly padded") << QStringLiteral("  alarmOnly  ")
                                      << int(Kind::AlarmOnly);
    QTest::newRow("unknown falls back")
        << QStringLiteral("bogus") << int(Kind::Guarantee);
    QTest::newRow("empty falls back") << QString() << int(Kind::Guarantee);
}

void TestVolumePolicy::kindParsing()
{
    QFETCH(QString, input);
    QFETCH(int, expected);

    QCOMPARE(int(VolumePolicy::kindFromString(input)), expected);
}

void TestVolumePolicy::kindToStringRoundTrip()
{
    QCOMPARE(VolumePolicy::kindToString(Kind::Guarantee),
             QStringLiteral("guarantee"));
    QCOMPARE(VolumePolicy::kindToString(Kind::AlarmOnly),
             QStringLiteral("alarmOnly"));
    QCOMPARE(VolumePolicy::kindFromString(
                 VolumePolicy::kindToString(Kind::AlarmOnly)),
             Kind::AlarmOnly);
    QCOMPARE(VolumePolicy::kindFromString(
                 VolumePolicy::kindToString(Kind::Guarantee)),
             Kind::Guarantee);
}

void TestVolumePolicy::alarmOnlyNeverTouches()
{
    const auto decision = VolumePolicy::decideRaise(
        Kind::AlarmOnly, {1000, 1000}, /*currentMute=*/true, /*floor=*/80);

    QVERIFY(!decision.touchSink);
    QVERIFY(!decision.unmute);
    QVERIFY(decision.targetVolumes.isEmpty());
}

void TestVolumePolicy::guaranteeSkipsWhenLoudEnough()
{
    // 80 % sink vs. a 70 % floor: nothing to do.
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {52429, 52429}, /*currentMute=*/false, /*floor=*/70);

    QVERIFY(!decision.touchSink);
    QVERIFY(!decision.unmute);
    QCOMPARE(decision.targetVolumes, QVector<int>({52429, 52429}));
}

void TestVolumePolicy::guaranteeUnmutesWithoutChangingVolume()
{
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {52429, 52429}, /*currentMute=*/true, /*floor=*/70);

    QVERIFY(decision.touchSink);
    QVERIFY(decision.unmute);
    QCOMPARE(decision.targetVolumes, QVector<int>({52429, 52429}));
}

void TestVolumePolicy::guaranteeRaisesBalancedSink()
{
    // 20 % sink to a 70 % floor: 70 * 65536 / 100 = 45875 raw.
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {13107, 13107}, /*currentMute=*/false, /*floor=*/70);

    QVERIFY(decision.touchSink);
    QVERIFY(!decision.unmute);
    QCOMPARE(decision.targetVolumes, QVector<int>({45875, 45875}));
    QVERIFY(decision.targetVolumes.at(0) <= VolumePolicy::kFullScale);
    QVERIFY(decision.targetVolumes.at(1) <= VolumePolicy::kFullScale);
}

void TestVolumePolicy::guaranteePreservesBalance()
{
    // 50 % / 75 % raised to a 60 % floor with ONE factor: the ratio 1.5 must
    // survive (39321, 58982) and neither channel may exceed 65536.
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {32768, 49152}, /*currentMute=*/false, /*floor=*/60);

    QVERIFY(decision.touchSink);
    QCOMPARE(decision.targetVolumes, QVector<int>({39321, 58982}));
    const double before = 49152.0 / 32768.0;
    const double after =
        double(decision.targetVolumes.at(1)) / decision.targetVolumes.at(0);
    QVERIFY(qAbs(before - after) < 0.01);
    QVERIFY(decision.targetVolumes.at(1) <= VolumePolicy::kFullScale);
}

void TestVolumePolicy::guaranteeClampsAtFullScale()
{
    // Floor 100 % on a 50 %/100 % sink: the factor (2.0) would push the loud
    // channel to 200 % - the guide forbids ever exceeding 100 %.
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {32768, 65536}, /*currentMute=*/false, /*floor=*/100);

    QVERIFY(decision.touchSink);
    QCOMPARE(decision.targetVolumes, QVector<int>({65536, 65536}));
    for (int volume : decision.targetVolumes)
        QVERIFY(volume <= VolumePolicy::kFullScale);
}

void TestVolumePolicy::guaranteeLiftsSilentChannel()
{
    // A silent channel cannot be scaled to the floor - it is lifted directly
    // and must NOT be scaled a second time by the factor pass.
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {0, 32768}, /*currentMute=*/false, /*floor=*/70);

    QVERIFY(decision.touchSink);
    QCOMPARE(decision.targetVolumes, QVector<int>({45875, 45875}));
}

void TestVolumePolicy::guaranteeFloorZeroIsNoOp()
{
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {1000, 2000}, /*currentMute=*/false, /*floor=*/0);

    QVERIFY(!decision.touchSink);
    QCOMPARE(decision.targetVolumes, QVector<int>({1000, 2000}));
}

void TestVolumePolicy::guaranteeHandlesEmptyState()
{
    const auto decision = VolumePolicy::decideRaise(
        Kind::Guarantee, {}, /*currentMute=*/false, /*floor=*/70);

    QVERIFY(!decision.touchSink);
    QVERIFY(decision.targetVolumes.isEmpty());
}

void TestVolumePolicy::streamClampNeverExceedsFullScale()
{
    QCOMPARE(VolumePolicy::clampStreamPercent(-5), 0);
    QCOMPARE(VolumePolicy::clampStreamPercent(0), 0);
    QCOMPARE(VolumePolicy::clampStreamPercent(50), 50);
    QCOMPARE(VolumePolicy::clampStreamPercent(100), 100);
    QCOMPARE(VolumePolicy::clampStreamPercent(150), 100);
}

void TestVolumePolicy::matchToleranceBoundary()
{
    // Guide: 3 % of 65536 = 1966 raw is still "the same" volume.
    QCOMPARE(VolumePolicy::kUserOverrideTolerance, 1966);

    QVERIFY(VolumePolicy::volumesMatch({10000, 10000}, {10000, 10000}));
    QVERIFY(VolumePolicy::volumesMatch({10000, 10000}, {11966, 10000}));
    QVERIFY(!VolumePolicy::volumesMatch({10000, 10000}, {11967, 10000}));
    QVERIFY(!VolumePolicy::volumesMatch({10000, 10000}, {8033, 10000}));
    QVERIFY(!VolumePolicy::volumesMatch({10000}, {10000, 10000}));
    QVERIFY(!VolumePolicy::volumesMatch({}, {}));
}

void TestVolumePolicy::userOverrideDetection_data()
{
    QTest::addColumn<QVector<int>>("observed");
    QTest::addColumn<bool>("observedMute");
    QTest::addColumn<bool>("isOverride");

    QTest::newRow("identical") << QVector<int>({65536, 65536}) << false
                               << false;
    QTest::newRow("within tolerance") << QVector<int>({63570, 65536}) << false
                                      << false;
    QTest::newRow("beyond tolerance")
        << QVector<int>({63569, 65536}) << false << true;
    QTest::newRow("user lowered it") << QVector<int>({1000, 1000}) << false
                                     << true;
    QTest::newRow("mute flipped") << QVector<int>({65536, 65536}) << true
                                  << true;
    QTest::newRow("nothing to compare") << QVector<int>() << false << true;
}

void TestVolumePolicy::userOverrideDetection()
{
    QFETCH(QVector<int>, observed);
    QFETCH(bool, observedMute);
    QFETCH(bool, isOverride);

    QCOMPARE(
        VolumePolicy::isUserOverride({65536, 65536}, /*appliedMute=*/false,
                                     observed, observedMute),
        isOverride);
}

QTEST_GUILESS_MAIN(TestVolumePolicy)

#include "tst_volumepolicy.moc"
