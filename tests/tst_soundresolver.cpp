#include "audio/SoundResolver.h"

#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

// Resolution chain: per-alarm sound -> global default -> first tone -> empty.
// Tone search uses injected dirs so the tests never depend on the machine's
// real tone folders.
class TestSoundResolver : public QObject
{
    Q_OBJECT

private slots:
    void perAlarmSoundWins();
    void globalDefaultUsedWhenPerAlarmMissing();
    void firstToneIsLastResort();
    void nothingResolvesToEmpty();
    void fullPathTakenAsIs();
    void nonAudioFilesAreNotTones();
    void defaultResolverHandlesPaths();

private:
    static void touch(const QString &path);
};

void TestSoundResolver::touch(const QString &path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
}

void TestSoundResolver::perAlarmSoundWins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("beep.wav")));
    touch(dir.filePath(QStringLiteral("chime.mp3")));

    const SoundResolver resolver({dir.path()});
    QCOMPARE(resolver.resolve(QStringLiteral("beep.wav"),
                              QStringLiteral("chime.mp3")),
             dir.filePath(QStringLiteral("beep.wav")));
}

void TestSoundResolver::globalDefaultUsedWhenPerAlarmMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("chime.mp3")));

    const SoundResolver resolver({dir.path()});
    QCOMPARE(resolver.resolve(QStringLiteral("missing.wav"),
                              QStringLiteral("chime.mp3")),
             dir.filePath(QStringLiteral("chime.mp3")));
    // Empty per-alarm sound (alarm without its own choice) as well.
    QCOMPARE(resolver.resolve(QString(), QStringLiteral("chime.mp3")),
             dir.filePath(QStringLiteral("chime.mp3")));
}

void TestSoundResolver::firstToneIsLastResort()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("chime.mp3")));
    touch(dir.filePath(QStringLiteral("beep.wav"))); // sorts first

    const SoundResolver resolver({dir.path()});
    QCOMPARE(resolver.resolve(QStringLiteral("missing.wav"),
                              QStringLiteral("also-missing.mp3")),
             dir.filePath(QStringLiteral("beep.wav")));
}

void TestSoundResolver::nothingResolvesToEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid()); // empty tone dir: ring without sound

    const SoundResolver resolver({dir.path()});
    QVERIFY(resolver.resolve(QStringLiteral("missing.wav"),
                             QStringLiteral("missing.mp3"))
                .isEmpty());
}

void TestSoundResolver::fullPathTakenAsIs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString absolute = dir.filePath(QStringLiteral("custom.wav"));
    touch(absolute);

    const SoundResolver resolver({dir.path()});
    QCOMPARE(resolver.resolve(absolute, QString()), absolute);

    // A dead absolute path falls through the whole chain: per-alarm misses,
    // default misses, and the first tone (custom.wav) rings instead.
    QCOMPARE(resolver.resolve(QStringLiteral("/nonexistent/x.wav"),
                              QString()),
             dir.filePath(QStringLiteral("custom.wav")));
}

void TestSoundResolver::nonAudioFilesAreNotTones()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("notes.txt"))); // not a tone

    const SoundResolver resolver({dir.path()});
    QVERIFY(resolver.resolve(QStringLiteral("missing.wav"),
                             QStringLiteral("missing.mp3"))
                .isEmpty());
}

void TestSoundResolver::defaultResolverHandlesPaths()
{
    // Default (real-directories) resolver: absolute paths need no search.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString absolute = dir.filePath(QStringLiteral("direct.ogg"));
    touch(absolute);

    const SoundResolver resolver;
    QCOMPARE(resolver.resolve(absolute, QString()), absolute);
}

QTEST_GUILESS_MAIN(TestSoundResolver)

#include "tst_soundresolver.moc"
