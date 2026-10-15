#include "audio/FakeMixer.h"
#include "audio/RestoreJournal.h"

#include <QtTest>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

// Contract of the crash-safe write-ahead journal:
//  - path follows $XDG_STATE_HOME (guide), atomic save, mode 0600, versioned;
//  - corrupt/unsupported files are logged and deleted, never fatal;
//  - recover() restores the original sink state only when the observed sink
//    still matches the applied one (compare-and-set, 3 % tolerance + mute),
//    clears the journal on success or deliberate skip, and KEEPS it on sink
//    errors so the next start retries.
// Recovery runs against FakeMixer, which answers synchronously.
class TestRestoreJournal : public QObject
{
    Q_OBJECT

private slots:
    void defaultPathFollowsXdgState();
    void recordAndLoadRoundTrip();
    void recordUsesPrivatePermissions();
    void recordReplacesPreviousEntry();
    void recordCreatesMissingParentDir();
    void recordFailsWhenParentIsNotADirectory();
    void recordRejectsIncompleteEntry();
    void loadMissingIsEmpty();
    void loadCorruptIsRemoved();
    void loadUnsupportedVersionIsRemoved();
    void loadIncompleteIsRemoved();
    void clearRemovesFile();

    void recoverWithoutJournal();
    void recoverRestoresWhenUnchanged();
    void recoverRestoresMute();
    void recoverSkipsWhenVolumeChanged();
    void recoverSkipsWhenMuteFlipped();
    void recoverKeepsJournalWhenSinkUnreadable();

private:
    static RestoreJournal::Entry makeEntry(
        const QString &sinkName = QStringLiteral("fake-sink"));
    static void writeRaw(const QString &path, const QByteArray &content);
};

RestoreJournal::Entry TestRestoreJournal::makeEntry(const QString &sinkName)
{
    RestoreJournal::Entry entry;
    entry.sinkName = sinkName;
    entry.originalVolumes = {52429, 52429}; // 80 % as found
    entry.originalMute = false;
    entry.appliedVolumes = {65536, 65536}; // what we are about to apply
    entry.appliedMute = false;
    return entry;
}

void TestRestoreJournal::writeRaw(const QString &path,
                                  const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
}

void TestRestoreJournal::defaultPathFollowsXdgState()
{
    const bool hadXdg = qEnvironmentVariableIsSet("XDG_STATE_HOME");
    const QByteArray previous = qgetenv("XDG_STATE_HOME");

    qputenv("XDG_STATE_HOME", QByteArray("/tmp/custom-state"));
    QCOMPARE(RestoreJournal::defaultPath(),
             QStringLiteral("/tmp/custom-state/lumalarm/audio-restore.json"));

    qunsetenv("XDG_STATE_HOME");
    QVERIFY(RestoreJournal::defaultPath().endsWith(
        QStringLiteral("/.local/state/lumalarm/audio-restore.json")));

    if (hadXdg)
        qputenv("XDG_STATE_HOME", previous);
}

void TestRestoreJournal::recordAndLoadRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    // 5.1 order must survive exactly (JSON objects would shuffle keys).
    RestoreJournal::Entry entry = makeEntry();
    entry.originalVolumes = {40000, 39000, 38000, 37000, 36000, 35000};
    entry.appliedVolumes = {45875, 45875, 45875, 45875, 45875, 45875};
    entry.originalMute = true;
    entry.appliedMute = false;

    QVERIFY(journal.record(entry));

    const auto loaded = journal.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->version, RestoreJournal::kCurrentVersion);
    QCOMPARE(loaded->sinkName, entry.sinkName);
    QCOMPARE(loaded->originalVolumes, entry.originalVolumes);
    QCOMPARE(loaded->originalMute, true);
    QCOMPARE(loaded->appliedVolumes, entry.appliedVolumes);
    QCOMPARE(loaded->appliedMute, false);
    QVERIFY(loaded->writtenAtMs > 0);
}

void TestRestoreJournal::recordUsesPrivatePermissions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));
    QVERIFY(journal.record(makeEntry()));

    const QFileDevice::Permissions permissions =
        QFile::permissions(journal.path());
    // 0600: owner may read+write, nobody else sees sink state on disk.
    QVERIFY(permissions & QFileDevice::ReadOwner);
    QVERIFY(permissions & QFileDevice::WriteOwner);
    QVERIFY(!(permissions & QFileDevice::ReadGroup));
    QVERIFY(!(permissions & QFileDevice::WriteGroup));
    QVERIFY(!(permissions & QFileDevice::ReadOther));
    QVERIFY(!(permissions & QFileDevice::WriteOther));
}

void TestRestoreJournal::recordReplacesPreviousEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    QVERIFY(journal.record(makeEntry()));

    RestoreJournal::Entry second = makeEntry();
    second.originalVolumes = {1000, 1000};
    QVERIFY(journal.record(second));

    const auto loaded = journal.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->originalVolumes, QVector<int>({1000, 1000}));
}

void TestRestoreJournal::recordCreatesMissingParentDir()
{
    // Phase 6 field report: a fresh machine has never created the app's
    // state directory, and QSaveFile cannot make it. record() must create
    // the parent itself — otherwise the write-ahead always fails, the
    // controller never touches the sink, and the floor silently does
    // nothing on every new install.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("lumalarm/audio-restore.json"));
    QVERIFY(!QFileInfo::exists(path));
    RestoreJournal journal(path);

    QVERIFY(journal.record(makeEntry()));
    QVERIFY(QDir().exists(dir.filePath(QStringLiteral("lumalarm"))));
    QVERIFY(QFile::exists(path));

    const auto loaded = journal.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->sinkName, makeEntry().sinkName);
}

void TestRestoreJournal::recordFailsWhenParentIsNotADirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A FILE where the parent directory should be: mkpath() must fail, so
    // record() reports failure and callers never touch the sink.
    const QString blocker = dir.filePath(QStringLiteral("blocker"));
    {
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
    RestoreJournal journal(blocker + QStringLiteral("/audio-restore.json"));

    // Write-ahead contract: a failed record() must be visible as false so
    // callers NEVER touch the sink without a journal.
    QVERIFY(!journal.record(makeEntry()));
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::recordRejectsIncompleteEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    RestoreJournal::Entry noName = makeEntry();
    noName.sinkName.clear();
    QVERIFY(!journal.record(noName));

    RestoreJournal::Entry noVolumes = makeEntry();
    noVolumes.originalVolumes.clear();
    QVERIFY(!journal.record(noVolumes));

    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::loadMissingIsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    QVERIFY(!journal.load().has_value());
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::loadCorruptIsRemoved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));
    writeRaw(journal.path(), QByteArray("definitely {not json"));

    QVERIFY(!journal.load().has_value());
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::loadUnsupportedVersionIsRemoved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    QJsonObject original;
    original.insert(QStringLiteral("volumes"), QJsonArray{52429, 52429});
    original.insert(QStringLiteral("mute"), false);
    QJsonObject root;
    root.insert(QStringLiteral("version"), 99);
    root.insert(QStringLiteral("sinkName"), QStringLiteral("fake-sink"));
    root.insert(QStringLiteral("original"), original);
    root.insert(QStringLiteral("applied"), original);
    writeRaw(journal.path(),
             QJsonDocument(root).toJson(QJsonDocument::Compact));

    QVERIFY(!journal.load().has_value());
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::loadIncompleteIsRemoved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    // Right version but missing the applied state: unusable, so deleted.
    QJsonObject original;
    original.insert(QStringLiteral("volumes"), QJsonArray{52429, 52429});
    original.insert(QStringLiteral("mute"), false);
    QJsonObject root;
    root.insert(QStringLiteral("version"), RestoreJournal::kCurrentVersion);
    root.insert(QStringLiteral("sinkName"), QStringLiteral("fake-sink"));
    root.insert(QStringLiteral("original"), original);
    writeRaw(journal.path(),
             QJsonDocument(root).toJson(QJsonDocument::Compact));

    QVERIFY(!journal.load().has_value());
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::clearRemovesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    QVERIFY(journal.record(makeEntry()));
    QVERIFY(journal.clear());
    QVERIFY(!QFile::exists(journal.path()));
    QVERIFY(journal.clear()); // idempotent
}

void TestRestoreJournal::recoverWithoutJournal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));
    FakeMixer mixer;

    QSignalSpy spy(&journal, &RestoreJournal::recoveryFinished);
    journal.recover(&mixer);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.first().at(0).toBool());
    QCOMPARE(mixer.applyVolumeCallCount(), 0);
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::recoverRestoresWhenUnchanged()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));
    QVERIFY(journal.record(makeEntry())); // original 80 %, applied 100 %

    // Crash happened after the apply: the sink still shows what we wrote.
    FakeMixer mixer;
    SinkState sink;
    sink.name = QStringLiteral("fake-sink");
    sink.volumes = {65536, 65536};
    sink.mute = false;
    mixer.setSink(sink);

    QSignalSpy spy(&journal, &RestoreJournal::recoveryFinished);
    journal.recover(&mixer);

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().at(0).toBool());
    QCOMPARE(mixer.lastAppliedVolumes(), QVector<int>({52429, 52429}));
    QCOMPARE(mixer.sink().volumes, QVector<int>({52429, 52429}));
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::recoverRestoresMute()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    RestoreJournal::Entry entry = makeEntry();
    entry.originalMute = true; // the sink was muted before we unmuted it
    QVERIFY(journal.record(entry));

    FakeMixer mixer;
    SinkState sink;
    sink.name = QStringLiteral("fake-sink");
    sink.volumes = {65536, 65536};
    sink.mute = false; // matches appliedMute
    mixer.setSink(sink);

    QSignalSpy spy(&journal, &RestoreJournal::recoveryFinished);
    journal.recover(&mixer);

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().at(0).toBool());
    QVERIFY(mixer.lastAppliedMute());
    QVERIFY(mixer.sink().mute);
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::recoverSkipsWhenVolumeChanged()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));
    QVERIFY(journal.record(makeEntry()));

    // The user moved the sink far beyond the 3 % tolerance: never fight
    // them, never restore, drop the journal.
    FakeMixer mixer;
    SinkState sink;
    sink.name = QStringLiteral("fake-sink");
    sink.volumes = {10000, 10000};
    sink.mute = false;
    mixer.setSink(sink);

    QSignalSpy spy(&journal, &RestoreJournal::recoveryFinished);
    journal.recover(&mixer);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.first().at(0).toBool());
    QCOMPARE(mixer.applyVolumeCallCount(), 0);
    QCOMPARE(mixer.sink().volumes, QVector<int>({10000, 10000}));
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::recoverSkipsWhenMuteFlipped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));
    QVERIFY(journal.record(makeEntry()));

    FakeMixer mixer;
    SinkState sink;
    sink.name = QStringLiteral("fake-sink");
    sink.volumes = {65536, 65536}; // matches the journal...
    sink.mute = true; // ...but the user muted it: their call, not ours
    mixer.setSink(sink);

    QSignalSpy spy(&journal, &RestoreJournal::recoveryFinished);
    journal.recover(&mixer);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.first().at(0).toBool());
    QCOMPARE(mixer.applyMuteCallCount(), 0);
    QVERIFY(mixer.sink().mute);
    QVERIFY(!QFile::exists(journal.path()));
}

void TestRestoreJournal::recoverKeepsJournalWhenSinkUnreadable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RestoreJournal journal(dir.filePath(QStringLiteral("audio-restore.json")));

    // The journaled sink cannot be read (unplugged device, busy server):
    // verification is impossible, so the journal must survive for a retry.
    QVERIFY(journal.record(makeEntry(QStringLiteral("ghost-sink"))));
    FakeMixer mixer; // holds "fake-sink" -> getSink("ghost-sink") errors

    QSignalSpy spy(&journal, &RestoreJournal::recoveryFinished);
    journal.recover(&mixer);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.first().at(0).toBool());
    QCOMPARE(mixer.applyVolumeCallCount(), 0);
    QVERIFY(QFile::exists(journal.path()));
}

QTEST_GUILESS_MAIN(TestRestoreJournal)

#include "tst_restorejournal.moc"
