#include "audio/MprisPause.h"

#include <QtTest>

#include <QDBusConnection>

// MprisPause: the pure bus-name filter plus the no-player/no-bus safety
// contract. Tests NEVER pause a real player: pauseAll() is only exercised
// when no session bus exists at all (headless CI).
class TestMprisPause : public QObject
{
    Q_OBJECT

private slots:
    void playerServicesFiltersBusNames();
    void playerServicesIgnoresUnrelatedNames();
    void pauseWithoutBusIsSafe();
    void resumeWithoutPlayersIsNoOp();
};

void TestMprisPause::playerServicesFiltersBusNames()
{
    const QStringList registered = {
        QStringLiteral("org.mpris.MediaPlayer2.spotify"),
        QStringLiteral("org.mpris.MediaPlayer2.vlc"),
        QStringLiteral("org.mpris.MediaPlayer2.mpv.instance1234"),
    };

    const QStringList players = MprisPause::playerServices(registered);

    QCOMPARE(players, QStringList({QStringLiteral("org.mpris.MediaPlayer2.mpv.instance1234"),
                                   QStringLiteral("org.mpris.MediaPlayer2.spotify"),
                                   QStringLiteral("org.mpris.MediaPlayer2.vlc")}));
}

void TestMprisPause::playerServicesIgnoresUnrelatedNames()
{
    const QStringList registered = {
        QStringLiteral("org.kde.plasma"),
        QStringLiteral("com.canonical.indicator.application"),
        QStringLiteral("org.mpris.MediaPlayer2"), // the spec name, no player
        QStringLiteral("org.mpris2.foo"),         // wrong prefix
    };

    QVERIFY(MprisPause::playerServices(registered).isEmpty());
    QVERIFY(MprisPause::playerServices({}).isEmpty());
}

void TestMprisPause::pauseWithoutBusIsSafe()
{
    if (QDBusConnection::sessionBus().isConnected())
        QSKIP("a session bus exists here: tests never pause real players");

    MprisPause pause;
    QVERIFY(!pause.sessionAvailable());
    pause.pauseAll(); // must be a pure no-op
    QVERIFY(pause.pausedPlayers().isEmpty());
}

void TestMprisPause::resumeWithoutPlayersIsNoOp()
{
    MprisPause pause;
    pause.resumeAll(); // nothing paused -> must not touch the bus
    QVERIFY(pause.pausedPlayers().isEmpty());
}

QTEST_GUILESS_MAIN(TestMprisPause)

#include "tst_mprispause.moc"
