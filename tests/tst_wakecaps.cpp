// WAKE-1 §A: capability detection parsed from FIXTURE sys-trees, never the
// live machine — the two no-assumption rules are asserted as data on every
// run: `mem` is not assumed to be S3, and rtc0 is not assumed to be the
// wake-capable RTC.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "wake/WakeCaps.h"

class TestWakeCaps : public QObject
{
    Q_OBJECT

private:
    // Materialise a fake /sys: path (relative to the root) -> file content.
    static QString makeTree(QTemporaryDir &dir, const QHash<QString, QString> &files)
    {
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const QString full = dir.filePath(it.key());
            QDir().mkpath(QFileInfo(full).path());
            QFile f(full);
            if (f.open(QIODevice::ReadOnly | QIODevice::WriteOnly | QIODevice::Text))
                f.write(it.value().toUtf8());
        }
        return dir.path();
    }

private slots:
    void parsesFullTree()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = makeTree(dir, {
                                               {QStringLiteral("power/state"),
                                                QStringLiteral("freeze standby mem disk\n")},
                                               {QStringLiteral("power/mem_sleep"),
                                                QStringLiteral("s2idle shallow [deep]\n")},
                                               {QStringLiteral("class/rtc/rtc0/name"),
                                                QStringLiteral("rtc0\n")},
                                               {QStringLiteral("class/rtc/rtc0/device/power/wakeup"),
                                                QStringLiteral("disabled\n")},
                                       });
        const auto caps = WakePolicy::readCapabilities(root);
        QCOMPARE(caps.sleepStates, QStringList({QStringLiteral("freeze"), QStringLiteral("standby"),
                                                QStringLiteral("mem"), QStringLiteral("disk")}));
        QCOMPARE(caps.memSleepTypes,
                 QStringList({QStringLiteral("s2idle"), QStringLiteral("shallow"),
                              QStringLiteral("deep")}));
        QCOMPARE(caps.memSleepDefault, QStringLiteral("deep"));
        QCOMPARE(caps.rtcNames, QStringList({QStringLiteral("rtc0")}));
        QVERIFY(caps.wakeCapableRtc.isEmpty()); // wakeup disabled -> not wake-capable
    }

    void memIsNeverAssumedToBeS3()
    {
        // A machine whose `mem` is only s2idle/shallow — no `deep` (S3) at all.
        QTemporaryDir dir;
        const QString root = makeTree(dir, {{QStringLiteral("power/mem_sleep"),
                                             QStringLiteral("s2idle [shallow]\n")}});
        const auto caps = WakePolicy::readCapabilities(root);
        QCOMPARE(caps.memSleepDefault, QStringLiteral("shallow"));
        QVERIFY(!caps.memSleepTypes.contains(QStringLiteral("deep")));
    }

    void wakeCapableRtcIsNotAssumedToBeRtc0()
    {
        QTemporaryDir dir;
        const QString root = makeTree(dir, {
                                               {QStringLiteral("class/rtc/rtc0/name"),
                                                QStringLiteral("rtc0\n")},
                                               {QStringLiteral("class/rtc/rtc0/device/power/wakeup"),
                                                QStringLiteral("disabled\n")},
                                               {QStringLiteral("class/rtc/rtc1/name"),
                                                QStringLiteral("rtc1\n")},
                                               {QStringLiteral("class/rtc/rtc1/device/power/wakeup"),
                                                QStringLiteral("enabled\n")},
                                       });
        const auto caps = WakePolicy::readCapabilities(root);
        QCOMPARE(caps.rtcNames, QStringList({QStringLiteral("rtc0"), QStringLiteral("rtc1")}));
        QCOMPARE(caps.wakeCapableRtc, QStringLiteral("rtc1"));
    }

    void rtcsAreOrderedNumericallyNotAsText()
    {
        QTemporaryDir dir;
        const QString root = makeTree(dir, {
                                               {QStringLiteral("class/rtc/rtc2/name"),
                                                QStringLiteral("rtc2\n")},
                                               {QStringLiteral("class/rtc/rtc10/name"),
                                                QStringLiteral("rtc10\n")},
                                       });
        const auto caps = WakePolicy::readCapabilities(root);
        QCOMPARE(caps.rtcNames, QStringList({QStringLiteral("rtc2"), QStringLiteral("rtc10")}));
    }

    void missingTreeYieldsEmptyNotCrash()
    {
        QTemporaryDir dir;
        const auto caps = WakePolicy::readCapabilities(dir.path());
        QVERIFY(caps.sleepStates.isEmpty());
        QVERIFY(caps.memSleepTypes.isEmpty());
        QVERIFY(caps.memSleepDefault.isEmpty());
        QVERIFY(caps.rtcNames.isEmpty());
        QVERIFY(caps.wakeCapableRtc.isEmpty());
    }

    void variantMapCarriesEveryKey()
    {
        const WakePolicy::Capabilities caps{QStringList{QStringLiteral("mem")},
                                            QStringList{QStringLiteral("s2idle")},
                                            QStringLiteral("s2idle"),
                                            QStringList{QStringLiteral("rtc0")},
                                            QStringLiteral("rtc0")};
        const QVariantMap map = WakePolicy::capabilitiesToVariantMap(caps);
        QCOMPARE(map.keys(), QStringList({QStringLiteral("memSleepDefault"),
                                          QStringLiteral("memSleepTypes"),
                                          QStringLiteral("rtcNames"), QStringLiteral("sleepStates"),
                                          QStringLiteral("wakeCapableRtc")}));
        QCOMPARE(map.value(QStringLiteral("wakeCapableRtc")).toString(), QStringLiteral("rtc0"));
    }
};

QTEST_MAIN(TestWakeCaps)
#include "tst_wakecaps.moc"
