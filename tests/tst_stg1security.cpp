#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>
#include <QtTest>

#include <cstdint>
#include <limits>
#include <unistd.h>

#include "wake/WakeSecurity.h"

#ifndef STG1_SOURCE_DIR
#define STG1_SOURCE_DIR "."
#endif
#ifndef STG1_BINARY_DIR
#define STG1_BINARY_DIR "."
#endif

// ============================================================================
// STG-1 security regression tests.
//
// Three tiers, clearly separated:
//
//  1. Static/contract tier — the shared boundary, the exact pkcheck argument
//     list, the packaging recipe and the D-Bus/polkit/systemd file contents.
//     Fully deterministic, no root, no bus, no systemd.
//
//  2. Install tier — a real DESTDIR install of the built tree, so "the package
//     contains the STG-1 helper and polkit policy" and "the package contains
//     no sudoers file" are asserted against actual install output rather than
//     against a CMakeLists.txt string.
//
//  3. Live tier — real D-Bus calls against the installed root helper: bounds,
//     replace semantics, rediscovery after a helper restart and non-root
//     denial. These QSKIP with an explicit reason when the system bus, an
//     installed helper, systemd or root are unavailable; they are never
//     reported as passing when they did not run.
// ============================================================================

namespace {

constexpr auto kService = "org.lumalarm.Helper";
constexpr auto kPath = "/org/lumalarm/Helper";
constexpr auto kInterface = "org.lumalarm.Helper";

QString readSourceFile(const QString &relative)
{
    QFile f(QString::fromLatin1(STG1_SOURCE_DIR) + QLatin1Char('/') + relative);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QStringList sourceLines(const QString &relative)
{
    return readSourceFile(relative).split(QLatin1Char('\n'));
}

bool commandExists(const QString &name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

// Base names of every armed Lumalarm transient timer, straight from systemd.
QStringList armedTimers()
{
    QProcess p;
    p.start(QStringLiteral("systemctl"),
            {QStringLiteral("list-units"), QStringLiteral("--all"), QStringLiteral("--no-legend"),
             QStringLiteral("--plain"), QStringLiteral("--full"),
             QStringLiteral("%1*.timer").arg(QLatin1String(WakePolicy::kTimerPrefix))});
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000))
        return QStringList();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return QStringList();
    return WakePolicy::armedTimerBases(QString::fromUtf8(p.readAllStandardOutput()));
}

bool runCommand(const QString &program, const QStringList &args, QString *output = nullptr,
                int timeoutMs = 60000, int *exitCode = nullptr)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(5000))
        return false;
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        return false;
    }
    // Both streams: callers assert on replies (stdout) and on diagnostics
    // (stderr), and a D-Bus error would otherwise be invisible to them.
    if (output)
        *output = QString::fromUtf8(p.readAllStandardOutput()) +
                  QString::fromUtf8(p.readAllStandardError());
    if (exitCode)
        *exitCode = p.exitCode();
    return p.exitStatus() == QProcess::NormalExit;
}

// busctl prints `<signature> <marshalled reply>` on its own line, e.g.
// `bs false "denied"`. systemd-run --scope adds a "Running as unit:" line to
// the other stream, so ignore those and return the reply itself.
QString busctlReply(const QString &output)
{
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        const QString t = raw.trimmed();
        if (t.startsWith(QStringLiteral("Running as unit:")))
            continue;
        const int sp = t.indexOf(QLatin1Char(' '));
        return sp < 0 ? QString() : t.mid(sp + 1).trimmed();
    }
    return QString();
}

// The source with comments removed, so static assertions test the CODE and
// not the prose that deliberately documents what is absent ("no system()",
// "the old sudoers rule is gone", ...).
QString sourceCodeOnly(const QString &relative)
{
    QString text = readSourceFile(relative);
    static const QRegularExpression block(QStringLiteral("/\\*.*?\\*/"),
                                          QRegularExpression::DotMatchesEverythingOption);
    text.remove(block);
    const QStringList lines = text.split(QLatin1Char('\n'));
    QStringList kept;
    for (QString line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("//")) || trimmed.startsWith(QStringLiteral("#")))
            continue;
        const int cut = line.indexOf(QStringLiteral(" //"));
        if (cut >= 0)
            line.truncate(cut);
        kept << line;
    }
    return kept.join(QLatin1Char('\n'));
}

// STG-1.6: every reference to a sudoers path in a packaging file must be
// either documentation or the removal of the pre-STG-1 rule. Nothing may
// install, edit, pipe into or grant anything through sudoers.
void assertNoSudoersMechanism(const QString &file)
{
    const QString text = readSourceFile(file);
    QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("%1 unreadable").arg(file)));

    // Not even as a command (documentation is exempt).
    QVERIFY2(!text.contains(QStringLiteral("visudo")),
             qPrintable(QStringLiteral("%1 edits sudoers with visudo").arg(file)));

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        const bool isComment = line.startsWith(QLatin1Char('#'));

        if (!isComment)
            QVERIFY2(
                !line.contains(QStringLiteral("sudo ")),
                qPrintable(QStringLiteral("%1:%2 runs sudo: %3").arg(file).arg(i + 1).arg(line)));

        if (!line.contains(QStringLiteral("sudoers")) && !line.contains(QStringLiteral("NOPASSWD")))
            continue;

        const bool isRemoval =
            line.startsWith(QStringLiteral("rm ")) && line.contains(QStringLiteral("/etc/sudoers"));
        const bool isExistenceTest = line.contains(QStringLiteral("[ -e /etc/sudoers"));
        const bool isHelperFunction = line.contains(QStringLiteral("_remove_legacy_sudoers"));
        const bool isPlainMessage =
            line.startsWith(QStringLiteral("echo ")) && !line.contains(QLatin1Char('>')) &&
            !line.contains(QStringLiteral("tee")) && !line.contains(QLatin1Char('|'));

        QVERIFY2(isComment || isRemoval || isExistenceTest || isHelperFunction || isPlainMessage,
                 qPrintable(QStringLiteral("%1:%2 sudoers reference is neither documentation "
                                           "nor a removal: %3")
                                .arg(file)
                                .arg(i + 1)
                                .arg(line)));

        if (isComment || isPlainMessage)
            continue;
        QVERIFY2(
            !line.contains(QStringLiteral("NOPASSWD")),
            qPrintable(
                QStringLiteral("%1:%2 writes a NOPASSWD rule: %3").arg(file).arg(i + 1).arg(line)));
        QVERIFY2(
            !line.contains(QStringLiteral("tee")),
            qPrintable(
                QStringLiteral("%1:%2 pipes into sudoers: %3").arg(file).arg(i + 1).arg(line)));
        QVERIFY2(
            !line.contains(QLatin1Char('>')),
            qPrintable(
                QStringLiteral("%1:%2 redirects into sudoers: %3").arg(file).arg(i + 1).arg(line)));
    }
}

// Owner of a process, from /proc — used to confirm that a test subject is
// really the unprivileged process the test asked systemd for.
uint procUid(uint pid)
{
    QFile f(QStringLiteral("/proc/%1/status").arg(pid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return ~0u;
    static const QRegularExpression uidRe(QStringLiteral("^Uid:\\s+(\\d+)"),
                                          QRegularExpression::MultilineOption);
    const QRegularExpressionMatch m = uidRe.match(QString::fromUtf8(f.readAll()));
    return m.hasMatch() ? m.captured(1).toUInt() : ~0u;
}

// A human-readable fingerprint of a subject, so an assertion failure says
// exactly which process polkit was asked about.
QString describeSubject(uint pid)
{
    QString detail;
    QFile comm(QStringLiteral("/proc/%1/comm").arg(pid));
    if (comm.open(QIODevice::ReadOnly | QIODevice::Text))
        detail += QStringLiteral(", comm=%1").arg(QString::fromUtf8(comm.readAll()).trimmed());
    detail += QStringLiteral(", uid=%1")
                  .arg(procUid(pid) == ~0u ? QStringLiteral("?") : QString::number(procUid(pid)));
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (cmdline.open(QIODevice::ReadOnly)) {
        QString c = QString::fromUtf8(cmdline.readAll());
        c.replace(QLatin1Char('\0'), QLatin1Char(' '));
        detail += QStringLiteral(", cmdline=[%1]").arg(c.trimmed());
    }
    QFile session(QStringLiteral("/proc/%1/sessionid").arg(pid));
    if (session.open(QIODevice::ReadOnly))
        detail +=
            QStringLiteral(", sessionid=%1").arg(QString::fromUtf8(session.readAll()).trimmed());
    return detail;
}

// One method call against the helper. `*transportOk` distinguishes "the helper
// answered" from "there was no answer at all".
QDBusMessage helperCall(const QString &method, const QVariantList &args = {},
                        bool *transportOk = nullptr)
{
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                       QString::fromLatin1(kInterface), method);
    msg.setArguments(args);
    // Long enough to outlast the helper's interactive polkit window, so a
    // challenge can never be mistaken for "helper unavailable".
    const QDBusMessage raw = QDBusConnection::systemBus().call(msg, QDBus::Block, 120000);
    if (transportOk)
        *transportOk = raw.type() != QDBusMessage::ErrorMessage;
    return raw;
}

bool helperStatus(QString *status)
{
    bool ok = false;
    const QDBusMessage raw = helperCall(QStringLiteral("GetStatus"), {}, &ok);
    if (!ok || raw.arguments().isEmpty())
        return false;
    if (status)
        *status = raw.arguments().first().toString();
    return true;
}

// Returns false when the transport failed; `success`/`message` carry the
// helper's own answer. The QVariant's type is what actually goes on the bus,
// so callers pick uint for the declared `u` signature and int to probe what
// happens with a value the signature does not admit.
bool helperSchedule(const QVariant &seconds, bool *success, QString *message)
{
    bool ok = false;
    const QDBusMessage raw = helperCall(QStringLiteral("ScheduleWake"), {seconds}, &ok);
    if (!ok || raw.arguments().size() < 2)
        return false;
    if (success)
        *success = raw.arguments().at(0).toBool();
    if (message)
        *message = raw.arguments().at(1).toString();
    return true;
}

bool helperCancel(bool *success, QString *message)
{
    bool ok = false;
    const QDBusMessage raw = helperCall(QStringLiteral("CancelWake"), {}, &ok);
    if (!ok || raw.arguments().size() < 2)
        return false;
    if (success)
        *success = raw.arguments().at(0).toBool();
    if (message)
        *message = raw.arguments().at(1).toString();
    return true;
}

// Preconditions for the live tier. `why` explains any refusal so a skip never
// looks like a pass.
bool liveHelperReady(QString *why)
{
    if (!QDBusConnection::systemBus().isConnected()) {
        *why = QStringLiteral("no system bus on this machine");
        return false;
    }
    if (geteuid() != 0) {
        *why = QStringLiteral("not running as root: a live ScheduleWake would raise a polkit "
                              "challenge dialog on the desktop mid-test-run");
        return false;
    }
    if (!commandExists(QStringLiteral("systemctl"))) {
        *why = QStringLiteral("systemctl unavailable");
        return false;
    }
    QDBusInterface iface(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                         QString::fromLatin1(kInterface), QDBusConnection::systemBus());
    if (!iface.isValid()) {
        *why = QStringLiteral("helper org.lumalarm.Helper not installed/activatable: %1")
                   .arg(iface.lastError().message());
        return false;
    }
    if (!armedTimers().isEmpty()) {
        *why = QStringLiteral("a Lumalarm wake is already armed on this machine");
        return false;
    }
    return true;
}

// Run pkcheck exactly the way the helper does (same shared argument builder).
int runPkcheck(const QStringList &extra, QString *output, int timeoutMs = 30000)
{
    QStringList args = extra;
    const QString pkcheck = QStandardPaths::findExecutable(QStringLiteral("pkcheck"));
    if (pkcheck.isEmpty())
        return -1;
    int code = -1;
    QString out;
    QProcess p;
    p.start(pkcheck, args);
    if (!p.waitForStarted(5000))
        return -1;
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        return -1;
    }
    out = QString::fromUtf8(p.readAllStandardOutput());
    const QString err = QString::fromUtf8(p.readAllStandardError());
    if (output)
        *output = out + err;
    code = p.exitCode();
    if (p.exitStatus() != QProcess::NormalExit)
        return -1;
    // pkcheck: 0 authorised, 1 not authorized, 2 interaction required,
    // 127 error. Preserve that distinction for the assertions.
    return code;
}

// First non-root PID that owns a unique system-bus name — the subject a real
// desktop caller would be, as far as polkit is concerned.
uint findNonRootBusPid()
{
    QProcess p;
    p.start(QStringLiteral("busctl"), {QStringLiteral("list"), QStringLiteral("--no-pager")});
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000))
        return 0;
    const QStringList lines =
        QString::fromUtf8(p.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList cols = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (cols.size() < 2 || !cols.at(1).contains(QRegularExpression(QStringLiteral("^\\d+$"))))
            continue;
        bool ok = false;
        const uint pid = cols.at(1).toUInt(&ok);
        if (!ok || pid <= 1)
            continue;
        QFile f(QStringLiteral("/proc/%1/status").arg(pid));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString status = QString::fromUtf8(f.readAll());
        const QRegularExpression uidRe(QStringLiteral("^Uid:\\s+(\\d+)"),
                                       QRegularExpression::MultilineOption);
        const QRegularExpressionMatch m = uidRe.match(status);
        if (m.hasMatch() && m.captured(1) != QLatin1String("0"))
            return pid;
    }
    return 0;
}

} // namespace

class TestStg1Security : public QObject
{
    Q_OBJECT

private slots:
    // ── Tier 1: deterministic contract checks ───────────────────────────
    void sharedBoundaryIsTheSingleSourceOfTruth();
    void pkcheckArgumentsEnableInteraction();
    void polkitActionIdMatchesThePolicyFile();
    void polkitFailsClosedOnUnknownSubject();
    void polkitAuthorizesRootFastPath();
    void polkitRequiresAuthenticationForNonRootSubject();
    void polkitDeniesSessionLessUnprivilegedSubject();
    void polkitChallengePathReachesTheAgent(); // opt-in, env-gated

    void pkgbuildInstallsNoSudoers();
    void pkgbuildLocalInstallsNoSudoers();
    void installHookNeverInstallsSudoers();
    void documentationGivesNoSudoersInstruction();

    void dbusAndPolkitFilesDefineTheExpectedContract();
    void cmakeInstallRulesCoverEveryStg1File();
    void helperSourceEnforcesTheBoundaryAndUsesNoShell();
    void wakeManagerHasNoSudoExecutionPath();

    // ── Tier 2: real install output ──────────────────────────────────────
    void installTreeContainsStg1FilesAndNoSudoers();

    // ── Tier 3: live helper behaviour ────────────────────────────────────
    void liveHelperRejectsOutOfBoundsRequests();
    void liveHelperAcceptsBoundaryAndReplacesThePreviousWake();
    void liveHelperRediscoversWakeAfterRestart();
    void liveNonRootCallerIsDeniedWhileStatusStaysPublic();
};

// ── Tier 1 ───────────────────────────────────────────────────────────────────

void TestStg1Security::sharedBoundaryIsTheSingleSourceOfTruth()
{
    QCOMPARE(WakePolicy::kMinWakeSeconds, 60);
    QCOMPARE(WakePolicy::kMaxWakeSeconds, 86400);

    const QList<qint64> rejected{
        0, 1, 59, 86401, 100000, qint64(UINT32_MAX), -1, -60, qint64(INT64_MIN)};
    for (const qint64 v : rejected)
        QVERIFY2(!WakePolicy::isValidWakeDelay(v),
                 qPrintable(QStringLiteral("%1 must be rejected").arg(v)));

    const QList<qint64> accepted{60, 61, 86400};
    for (const qint64 v : accepted)
        QVERIFY2(WakePolicy::isValidWakeDelay(v),
                 qPrintable(QStringLiteral("%1 must be accepted").arg(v)));
}

void TestStg1Security::pkcheckArgumentsEnableInteraction()
{
    const QStringList withName = WakePolicy::pkcheckArguments(QStringLiteral(":1.42"), 0);
    QVERIFY(withName.contains(QStringLiteral("--allow-user-interaction")));
    QVERIFY(withName.contains(QStringLiteral("--action-id")));
    QVERIFY(withName.contains(QString::fromLatin1(WakePolicy::kPolkitActionId)));
    QVERIFY(withName.contains(QStringLiteral("--system-bus-name")));
    QVERIFY(withName.contains(QStringLiteral(":1.42")));
    // Exactly one subject: a bus name and a PID together is ambiguous.
    QVERIFY(!withName.contains(QStringLiteral("--process")));

    const QStringList withPid = WakePolicy::pkcheckArguments(QString(), 1234);
    QVERIFY(withPid.contains(QStringLiteral("--allow-user-interaction")));
    QVERIFY(withPid.contains(QStringLiteral("--process")));
    QVERIFY(withPid.contains(QStringLiteral("1234")));
    QVERIFY(!withPid.contains(QStringLiteral("--system-bus-name")));
}

void TestStg1Security::polkitActionIdMatchesThePolicyFile()
{
    const QString policy = readSourceFile(QStringLiteral("data/org.lumalarm.schedule.policy"));
    QVERIFY2(!policy.isEmpty(), "policy file unreadable");
    const QString actionId = QString::fromLatin1(WakePolicy::kPolkitActionId);
    QVERIFY(policy.contains(QStringLiteral("action id=\"%1\"").arg(actionId)));

    // STG-1.2: the policy must still describe an authentication requirement —
    // and must not silently allow everybody.
    QVERIFY(policy.contains(QStringLiteral("<allow_active>auth_admin_keep</allow_active>")));
    QVERIFY(policy.contains(QStringLiteral("<allow_any>no</allow_any>")));
    QVERIFY(policy.contains(QStringLiteral("<allow_inactive>no</allow_inactive>")));

    // The action id lives in exactly one place — the shared header — and it is
    // what both the helper's pkcheckArguments() and this test read. The helper
    // must not carry a private copy that could drift.
    const QString security = readSourceFile(QStringLiteral("src/wake/WakeSecurity.h"));
    QVERIFY2(!security.isEmpty(), "WakeSecurity.h unreadable");
    QVERIFY(security.contains(QStringLiteral("kPolkitActionId = \"%1\"").arg(actionId)));

    const QString helper = readSourceFile(QStringLiteral("helper/lumalarm-wake-helper.cpp"));
    QVERIFY2(!helper.isEmpty(), "helper source unreadable");
    QVERIFY(helper.contains(QStringLiteral("WakePolicy::pkcheckArguments")));
    QVERIFY2(!sourceCodeOnly(QStringLiteral("helper/lumalarm-wake-helper.cpp")).contains(actionId),
             "the helper must not hard-code its own copy of the polkit action id");
}

void TestStg1Security::polkitFailsClosedOnUnknownSubject()
{
    if (QStandardPaths::findExecutable(QStringLiteral("pkcheck")).isEmpty())
        QSKIP("pkcheck is not installed on this machine");

    // A subject that does not exist must never be authorized.
    const QStringList args = WakePolicy::pkcheckArguments(QString(), 999999u);
    QString out;
    const int rc = runPkcheck(args, &out);
    QVERIFY(rc != 0);
    QVERIFY2(rc == 1 || rc == 2 || rc == 127,
             qPrintable(QStringLiteral("unexpected pkcheck exit %1: %2").arg(rc).arg(out)));
}

void TestStg1Security::polkitAuthorizesRootFastPath()
{
    if (QStandardPaths::findExecutable(QStringLiteral("pkcheck")).isEmpty())
        QSKIP("pkcheck is not installed on this machine");
    if (geteuid() != 0)
        QSKIP("not running as root — the root fast path cannot be observed here");

    // Root is the documented fast path; it must not be blocked by the
    // interaction flag either.
    QString out;
    const int rc = runPkcheck(WakePolicy::pkcheckArguments(QString(), uint(getpid())), &out);
    QVERIFY2(rc == 0,
             qPrintable(QStringLiteral("root was not authorized: rc=%1 %2").arg(rc).arg(out)));
}

void TestStg1Security::polkitRequiresAuthenticationForNonRootSubject()
{
    if (QStandardPaths::findExecutable(QStringLiteral("pkcheck")).isEmpty())
        QSKIP("pkcheck is not installed on this machine");

    const uint pid = findNonRootBusPid();
    if (pid == 0)
        QSKIP("no non-root process with a system-bus name on this machine");

    // Deliberately WITHOUT --allow-user-interaction: that is both the
    // non-intrusive form (no dialog) and the one that exposes what the policy
    // really demands for this subject. A normal desktop user must land on 2
    // ("interaction required") or 1 ("not authorized") — never 0, which is
    // reserved for the root fast path. This is the "a normal desktop user
    // reaches the polkit path; it is never silently fast-tracked" half of
    // STG-1.2.
    QString out;
    const int rc = runPkcheck(QStringList{QStringLiteral("--action-id"),
                                          QString::fromLatin1(WakePolicy::kPolkitActionId),
                                          QStringLiteral("--process"), QString::number(pid)},
                              &out);
    if (rc == 0)
        QSKIP(qPrintable(QStringLiteral("pkcheck authorized non-root pid %1 without a "
                                        "challenge — a temporary authorization from "
                                        "auth_admin_keep may still be cached: %2")
                             .arg(pid)
                             .arg(out.trimmed())));

    QVERIFY2(rc == 1 || rc == 2 || rc == 127,
             qPrintable(QStringLiteral("unexpected pkcheck result %1 for non-root pid %2: %3")
                            .arg(rc)
                            .arg(pid)
                            .arg(out.trimmed())));
}

void TestStg1Security::polkitDeniesSessionLessUnprivilegedSubject()
{
    if (QStandardPaths::findExecutable(QStringLiteral("pkcheck")).isEmpty())
        QSKIP("pkcheck is not installed on this machine");
    if (!commandExists(QStringLiteral("systemd-run")) ||
        !commandExists(QStringLiteral("systemctl")))
        QSKIP("systemd-run/systemctl unavailable — cannot build a session-less subject");

    // An unprivileged process started by PID 1 has no logind session, so
    // polkit evaluates it under allow_inactive=no and denies it immediately —
    // even with --allow-user-interaction. Deterministic: no dialog, no
    // dependence on the desktop's authentication agent.
    const QString unit =
        QStringLiteral("stg1-probe-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString unitName = unit + QStringLiteral(".service");

    QString out;
    int rc = -1;
    const bool started = runCommand(QStringLiteral("systemd-run"),
                                    {QStringLiteral("--unit=") + unit, QStringLiteral("--collect"),
                                     QStringLiteral("--uid=65534"), QStringLiteral("--gid=65534"),
                                     QStringLiteral("/bin/sleep"), QStringLiteral("90")},
                                    &out, 30000, &rc);
    if (!started || rc != 0)
        QSKIP(qPrintable(
            QStringLiteral("could not start a session-less probe subject: %1").arg(out.trimmed())));

    // Resolve the probe's pid, re-reading until systemd reports the
    // *unprivileged* main process that was actually exec'd. There is a window
    // in which the transient unit is queued but has not started, and trusting
    // a stale MainPID would silently turn this into a root fast-path check.
    QString unitState;
    uint probePid = 0;
    for (int attempt = 0; attempt < 20 && probePid == 0; ++attempt) {
        QString pidOut;
        int pidRc = -1;
        const bool havePid = runCommand(
            QStringLiteral("systemctl"),
            {QStringLiteral("show"), unitName, QStringLiteral("--property=User"),
             QStringLiteral("--property=MainPID"), QStringLiteral("--property=ActiveState"),
             QStringLiteral("--property=Result")},
            &pidOut, 15000, &pidRc);
        unitState = pidOut.simplified();
        static const QRegularExpression mainPidRe(QStringLiteral("MainPID=(\\d+)"));
        const QRegularExpressionMatch m = mainPidRe.match(pidOut);
        const uint candidate = m.hasMatch() ? m.captured(1).toUInt() : 0u;
        if (havePid && pidRc == 0 && candidate > 1 && procUid(candidate) == 65534)
            probePid = candidate;
        else
            QTest::qWait(250);
    }

    if (probePid == 0) {
        runCommand(QStringLiteral("systemctl"), {QStringLiteral("stop"), unitName}, nullptr, 30000);
        QSKIP(qPrintable(QStringLiteral("could not resolve an unprivileged probe subject "
                                        "(unit start: %1; unit state: %2)")
                             .arg(out.trimmed(), unitState)));
    }

    const QString subjectDetail = describeSubject(probePid);

    // pkcheck with the helper's exact argument list, interaction allowed.
    QString deniedOut;
    const int denied =
        runPkcheck(WakePolicy::pkcheckArguments(QString(), probePid), &deniedOut, 45000);

    runCommand(QStringLiteral("systemctl"), {QStringLiteral("stop"), unitName}, nullptr, 30000);

    QVERIFY2(denied != 0,
             qPrintable(QStringLiteral("polkit authorized a session-less unprivileged subject "
                                       "(pid %1%2, unit state: %3, pkcheck rc=%4): %5")
                            .arg(probePid)
                            .arg(subjectDetail, unitState)
                            .arg(denied)
                            .arg(deniedOut.trimmed())));
}

void TestStg1Security::polkitChallengePathReachesTheAgent()
{
    // Deliberately opt-in: proving the challenge path end-to-end makes the
    // desktop polkit agent flash an authentication dialog for a few seconds.
    if (!qEnvironmentVariableIsSet("STG1_LIVE_POLKIT_CHALLENGE"))
        QSKIP("set STG1_LIVE_POLKIT_CHALLENGE=1 to run the interactive challenge check "
              "(it shows a desktop authentication dialog briefly)");
    if (QStandardPaths::findExecutable(QStringLiteral("pkcheck")).isEmpty())
        QSKIP("pkcheck is not installed on this machine");

    const uint pid = findNonRootBusPid();
    if (pid == 0)
        QSKIP("no non-root process with a system-bus name on this machine");

    QString noInteractionOut;
    const int without = runPkcheck(QStringList{QStringLiteral("--action-id"),
                                               QString::fromLatin1(WakePolicy::kPolkitActionId),
                                               QStringLiteral("--process"), QString::number(pid)},
                                   &noInteractionOut);
    if (without != 2)
        QSKIP(qPrintable(QStringLiteral("pid %1 did not report 'interaction required' "
                                        "without the flag (rc=%2): %3")
                             .arg(pid)
                             .arg(without)
                             .arg(noInteractionOut.trimmed())));

    // The F2 defect in one assertion: without --allow-user-interaction pkcheck
    // refuses outright (rc 2). With the helper's exact arguments it must get
    // past that refusal and reach the agent.
    QString out;
    const int withInteraction =
        runPkcheck(WakePolicy::pkcheckArguments(QString(), pid), &out, 45000);
    QVERIFY2(withInteraction != 2,
             qPrintable(QStringLiteral("pkcheck still refuses to interact with the agent "
                                       "using the helper's arguments: rc=%1 %2")
                            .arg(withInteraction)
                            .arg(out.trimmed())));
}

void TestStg1Security::pkgbuildInstallsNoSudoers()
{
    // STG-1.6: no sudoers mechanism of any kind may survive in the recipe.
    assertNoSudoersMechanism(QStringLiteral("PKGBUILD"));

    const QString pkgbuild = readSourceFile(QStringLiteral("PKGBUILD"));
    QVERIFY2(!pkgbuild.contains(QStringLiteral("/etc/sudoers")),
             "PKGBUILD must not touch /etc/sudoers");
    QVERIFY2(!pkgbuild.contains(QStringLiteral("NOPASSWD")),
             "PKGBUILD must not install a NOPASSWD rule");

    // The recipe must build the current STG-1 application: no GitHub archive
    // source (no tag carries the helper or the polkit policy), and the D-Bus
    // helper must be part of what it packages.
    QVERIFY2(!pkgbuild.contains(QStringLiteral("archive/v")),
             "PKGBUILD must not source a release tarball that predates STG-1");
    QVERIFY(pkgbuild.contains(QStringLiteral("install=lumalarm.install")));
    QVERIFY(pkgbuild.contains(QStringLiteral("lumalarm-wake-helper")));

    // Inline hooks are makepkg no-ops, so the packaging contract must live in
    // lumalarm.install and nowhere else.
    static const QRegularExpression inlineHook(
        QStringLiteral("(?m)^\\s*(post_install|post_upgrade|pre_install|pre_upgrade|"
                       "pre_remove|post_remove)\\s*\\(\\)"));
    QVERIFY2(!inlineHook.match(pkgbuild).hasMatch(),
             "PKGBUILD must not carry inline hooks (makepkg ignores them)");
}

void TestStg1Security::pkgbuildLocalInstallsNoSudoers()
{
    assertNoSudoersMechanism(QStringLiteral("PKGBUILD.local"));
}

void TestStg1Security::installHookNeverInstallsSudoers()
{
    // The hook may only *remove* the pre-STG-1 rule, never create one.
    assertNoSudoersMechanism(QStringLiteral("lumalarm.install"));

    const QString install = readSourceFile(QStringLiteral("lumalarm.install"));
    const QStringList lines = install.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('#')))
            continue;
        QVERIFY2(!line.contains(QStringLiteral("NOPASSWD")),
                 qPrintable(QStringLiteral("active NOPASSWD reference: %1").arg(line)));
        QVERIFY2(!line.contains(QStringLiteral("wheel")),
                 qPrintable(QStringLiteral("active wheel grant: %1").arg(line)));
    }

    // And it must really perform the removal it advertises, so an upgraded
    // install actually loses the old rule.
    QVERIFY2(install.contains(QStringLiteral("rm -f /etc/sudoers.d/lumalarm")),
             "lumalarm.install must remove the legacy /etc/sudoers.d/lumalarm rule");
}

void TestStg1Security::documentationGivesNoSudoersInstruction()
{
    // Nobody may be told how to create the rule any more. Mentions that
    // describe its removal are fine; instructions to write it are not.
    const QStringList docs{QStringLiteral("README.md"), QStringLiteral("CONTRIBUTING.md"),
                           QStringLiteral("CHANGELOG.md")};
    for (const QString &doc : docs) {
        const QString text = readSourceFile(doc);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("%1 unreadable").arg(doc)));
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const bool mentionsRule = line.contains(QStringLiteral("sudoers"));
            if (!mentionsRule)
                continue;
            QVERIFY2(!line.contains(QStringLiteral("tee")),
                     qPrintable(QStringLiteral("%1 still tells the user to write a sudoers "
                                               "file: %2")
                                    .arg(doc, line.trimmed())));
            QVERIFY2(!line.contains(QStringLiteral("visudo")),
                     qPrintable(QStringLiteral("%1 still tells the user to edit sudoers: %2")
                                    .arg(doc, line.trimmed())));
            QVERIFY2(!line.contains(QStringLiteral("NOPASSWD: /usr/bin/rtcwake")) ||
                         line.contains(QStringLiteral("removes")) ||
                         line.contains(QStringLiteral("old")) ||
                         line.contains(QStringLiteral("replaces")) ||
                         line.contains(QStringLiteral("Removed")),
                     qPrintable(QStringLiteral("%1 still documents the rtcwake sudoers rule: %2")
                                    .arg(doc, line.trimmed())));
        }
    }
}

void TestStg1Security::dbusAndPolkitFilesDefineTheExpectedContract()
{
    const QString service = readSourceFile(QStringLiteral("data/org.lumalarm.Helper.service"));
    QVERIFY(!service.isEmpty());
    QVERIFY(service.contains(QStringLiteral("Exec=/usr/lib/lumalarm/lumalarm-wake-helper")));
    QVERIFY(service.contains(QStringLiteral("SystemdService=lumalarm-wake-helper.service")));

    const QString unit = readSourceFile(QStringLiteral("data/lumalarm-wake-helper.service"));
    QVERIFY(!unit.isEmpty());
    QVERIFY(unit.contains(QStringLiteral("Type=dbus")));
    QVERIFY(unit.contains(QStringLiteral("ExecStart=/usr/lib/lumalarm/lumalarm-wake-helper")));

    const QString xml = readSourceFile(QStringLiteral("data/org.lumalarm.Helper.xml"));
    QVERIFY(!xml.isEmpty());
    QVERIFY(xml.contains(QStringLiteral("ScheduleWake")));
    QVERIFY(xml.contains(QStringLiteral("CancelWake")));
    QVERIFY(xml.contains(QStringLiteral("GetStatus")));

    const QString policy = readSourceFile(QStringLiteral("data/org.lumalarm.schedule.policy"));
    QVERIFY(!policy.isEmpty());
    QVERIFY(policy.contains(QString::fromLatin1(WakePolicy::kPolkitActionId)));
}

void TestStg1Security::cmakeInstallRulesCoverEveryStg1File()
{
    const QString cmake = readSourceFile(QStringLiteral("CMakeLists.txt"));
    QVERIFY(!cmake.isEmpty());
    const QStringList required{
        QStringLiteral("install(TARGETS lumalarm-wake-helper"),
        QStringLiteral("data/org.lumalarm.Helper.service"),
        QStringLiteral("data/org.lumalarm.Helper.conf"),
        QStringLiteral("data/org.lumalarm.Helper.xml"),
        QStringLiteral("data/org.lumalarm.schedule.policy"),
        QStringLiteral("data/lumalarm-wake-helper.service"),
    };
    for (const QString &needle : required)
        QVERIFY2(
            cmake.contains(needle),
            qPrintable(
                QStringLiteral("CMakeLists.txt is missing an STG-1 install rule: %1").arg(needle)));
}

void TestStg1Security::helperSourceEnforcesTheBoundaryAndUsesNoShell()
{
    const QString helper = readSourceFile(QStringLiteral("helper/lumalarm-wake-helper.cpp"));
    QVERIFY(!helper.isEmpty());

    // The boundary comes from the shared header — never a private copy.
    QVERIFY(helper.contains(QStringLiteral("#include \"wake/WakeSecurity.h\"")));
    QVERIFY(helper.contains(QStringLiteral("WakePolicy::isValidWakeDelay")));
    QVERIFY(!helper.contains(QStringLiteral("kMinSeconds =")));
    QVERIFY(!helper.contains(QStringLiteral("kMaxSeconds =")));

    // STG-1.2: the interaction flag must actually be passed (through the
    // shared argument builder), otherwise auth_admin_keep is unreachable.
    QVERIFY(helper.contains(QStringLiteral("WakePolicy::pkcheckArguments")));

    // STG-1.4: state must be rediscovered from systemd, not memory.
    QVERIFY(helper.contains(QStringLiteral("rediscoverState")));
    QVERIFY(helper.contains(QStringLiteral("armedTimerBasesFromSystemd")));

    // No shell, no privilege escalation in the privileged component. Checked
    // against comment-free source so the prose that documents what is absent
    // ("no system()", "the old sudoers rule") cannot mask or fake a result.
    const QString code = sourceCodeOnly(QStringLiteral("helper/lumalarm-wake-helper.cpp"));
    QVERIFY2(!code.contains(QStringLiteral("system(")),
             "the privileged helper must never call system()");
    QVERIFY2(!code.contains(QStringLiteral("popen(")),
             "the privileged helper must never call popen()");
    QVERIFY2(!code.contains(QStringLiteral("/bin/sh")),
             "the privileged helper must never invoke a shell");
    QVERIFY2(!code.contains(QStringLiteral("bash -c")),
             "the privileged helper must never invoke bash -c");
    QVERIFY2(!code.contains(QStringLiteral("sudo")), "the privileged helper must never run sudo");
    QVERIFY2(!code.contains(QStringLiteral("pkexec")),
             "the privileged helper must never run pkexec");
}

void TestStg1Security::wakeManagerHasNoSudoExecutionPath()
{
    const QString wm = readSourceFile(QStringLiteral("src/wake/WakeManager.cpp"));
    QVERIFY(!wm.isEmpty());
    const QString wmCode = sourceCodeOnly(QStringLiteral("src/wake/WakeManager.cpp"));

    // STG-1.5: no sudo anywhere in the wake/poweroff paths — checked on
    // comment-free code, so a removed call cannot linger behind prose.
    QVERIFY2(!wmCode.contains(QStringLiteral("sudo")), "WakeManager.cpp must not spawn sudo");
    QVERIFY2(!wmCode.contains(QStringLiteral("pkexec")), "WakeManager.cpp must not spawn pkexec");
    QVERIFY2(!wmCode.contains(QStringLiteral("system(")), "WakeManager.cpp must not call system()");
    QVERIFY2(!wmCode.contains(QStringLiteral("popen(")), "WakeManager.cpp must not call popen()");

    // The validation boundary must be referenced from the shared header.
    QVERIFY(wm.contains(QStringLiteral("#include \"wake/WakeSecurity.h\"")));
    QVERIFY(wm.contains(QStringLiteral("WakePolicy::isValidWakeDelay")));

    const QString header = readSourceFile(QStringLiteral("src/wake/WakeManager.h"));
    QVERIFY(!header.isEmpty());
    QVERIFY(header.contains(QStringLiteral("WakePolicy::kMinWakeSeconds")));
}

// ── Tier 2 ───────────────────────────────────────────────────────────────────

void TestStg1Security::installTreeContainsStg1FilesAndNoSudoers()
{
    if (!commandExists(QStringLiteral("cmake")))
        QSKIP("cmake unavailable");

    QTemporaryDir destDir;
    QVERIFY2(destDir.isValid(), "could not create a temporary DESTDIR");

    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("DESTDIR"), destDir.path());
    p.setProcessEnvironment(env);
    p.start(QStandardPaths::findExecutable(QStringLiteral("cmake")),
            {QStringLiteral("--install"), QStringLiteral(STG1_BINARY_DIR)});
    QVERIFY2(p.waitForStarted(5000), "cmake --install did not start");
    QVERIFY2(p.waitForFinished(300000), "cmake --install timed out");
    const QString log =
        QString::fromUtf8(p.readAllStandardOutput()) + QString::fromUtf8(p.readAllStandardError());
    QVERIFY2(p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
             qPrintable(QStringLiteral("cmake --install failed:\n%1").arg(log)));

    const QString root = destDir.path();
    const QStringList required{
        QStringLiteral("/usr/lib/lumalarm/lumalarm-wake-helper"),
        QStringLiteral("/usr/share/dbus-1/system-services/org.lumalarm.Helper.service"),
        QStringLiteral("/usr/share/dbus-1/system.d/org.lumalarm.Helper.conf"),
        QStringLiteral("/usr/share/polkit-1/actions/org.lumalarm.schedule.policy"),
        QStringLiteral("/usr/lib/systemd/system/lumalarm-wake-helper.service"),
        QStringLiteral("/usr/share/lumalarm/org.lumalarm.Helper.xml"),
    };
    for (const QString &path : required)
        QVERIFY2(QFileInfo::exists(root + path),
                 qPrintable(QStringLiteral("missing from the install tree: %1").arg(path)));

    // The whole point of STG-1.6: nothing sudoers-shaped is ever installed.
    QVERIFY2(!QFileInfo::exists(root + QStringLiteral("/etc/sudoers.d/lumalarm")),
             "the install tree must not contain a sudoers file");
    const QDir sudoersDir(root + QStringLiteral("/etc/sudoers.d"));
    const QStringList sudoersFiles =
        sudoersDir.exists() ? sudoersDir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)
                            : QStringList();
    QVERIFY2(sudoersFiles.isEmpty(), qPrintable(QStringLiteral("unexpected sudoers files: %1")
                                                    .arg(sudoersFiles.join(QLatin1Char(',')))));

    // And no sudoers-named file anywhere else in the install output.
    QStringList anywhere;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        if (f.contains(QStringLiteral("sudoers")))
            anywhere << f;
    }
    QVERIFY2(
        anywhere.isEmpty(),
        qPrintable(
            QStringLiteral("sudoers files installed: %1").arg(anywhere.join(QLatin1Char(',')))));
}

// ── Tier 3 ───────────────────────────────────────────────────────────────────

void TestStg1Security::liveHelperRejectsOutOfBoundsRequests()
{
    QString why;
    if (!liveHelperReady(&why))
        QSKIP(qPrintable(why));

    QString statusBefore;
    QVERIFY(helperStatus(&statusBefore));
    QCOMPARE(statusBefore, QStringLiteral("idle"));
    QVERIFY(armedTimers().isEmpty());

    const QList<qint64> invalid{0, 1, 59, 86401, 100000, qint64(UINT32_MAX), -1};
    for (const qint64 v : invalid) {
        if (v < 0 || v > qint64(UINT32_MAX)) // outside the u D-Bus type
            continue;
        bool success = true;
        QString message;
        QVERIFY2(helperSchedule(uint(v), &success, &message),
                 qPrintable(QStringLiteral("transport failure for %1").arg(v)));
        QVERIFY2(!success, qPrintable(QStringLiteral("out-of-range value %1 was accepted").arg(v)));
        QCOMPARE(message, QStringLiteral("out of range"));
    }

    // Negative input, where the API admits none: the declared signature is
    // `u`. Whether the bus refuses the mismatched type outright or the value
    // is delivered, the only acceptable outcome is "no wake".
    {
        bool negativeSuccess = true;
        QString negativeMessage;
        const bool answered = helperSchedule(QVariant(int(-1)), &negativeSuccess, &negativeMessage);
        if (answered)
            QVERIFY2(!negativeSuccess,
                     qPrintable(
                         QStringLiteral("a negative delay was accepted: %1").arg(negativeMessage)));
        QVERIFY2(armedTimers().isEmpty(), "a negative delay created a wake timer");
    }

    // Nothing was created or changed by any of those rejections.
    QString statusAfter;
    QVERIFY(helperStatus(&statusAfter));
    QCOMPARE(statusAfter, statusBefore);
    QVERIFY2(armedTimers().isEmpty(), "an out-of-range request created a wake timer");
}

void TestStg1Security::liveHelperAcceptsBoundaryAndReplacesThePreviousWake()
{
    QString why;
    if (!liveHelperReady(&why))
        QSKIP(qPrintable(why));

    bool success = false;
    QString message;

    // Lower bound. (The literal must be an unsigned int so the argument is
    // marshalled with the declared `u` signature.)
    QVERIFY(helperSchedule(60u, &success, &message));
    QVERIFY2(success, qPrintable(message));
    QCOMPARE(armedTimers().size(), 1);
    QString status;
    QVERIFY(helperStatus(&status));
    QCOMPARE(status, QStringLiteral("scheduled:60"));

    // Duplicate scheduling replaces: still exactly one Lumalarm wake.
    QVERIFY(helperSchedule(86400u, &success, &message));
    QVERIFY2(success, qPrintable(message));
    QCOMPARE(armedTimers().size(), 1);
    QVERIFY(helperStatus(&status));
    QVERIFY2(status.startsWith(QStringLiteral("scheduled")),
             qPrintable(QStringLiteral("unexpected status after re-schedule: %1").arg(status)));
    QVERIFY(status.contains(QStringLiteral("86400")));

    // Cancellation is idempotent and truthful.
    QVERIFY(helperCancel(&success, &message));
    QVERIFY2(success, qPrintable(message));
    QVERIFY2(armedTimers().isEmpty(), "the timer survived cancellation");
    QVERIFY(helperStatus(&status));
    QCOMPARE(status, QStringLiteral("idle"));

    // A second cancel has nothing to do and says so — without failing.
    QVERIFY(helperCancel(&success, &message));
    QVERIFY(success);
    QCOMPARE(message, QStringLiteral("idle"));

    if (!armedTimers().isEmpty()) // belt and braces for a failed assertion
        helperCancel(&success, &message);
}

void TestStg1Security::liveHelperRediscoversWakeAfterRestart()
{
    QString why;
    if (!liveHelperReady(&why))
        QSKIP(qPrintable(why));
    if (geteuid() != 0)
        QSKIP("restarting the root helper requires root");

    bool success = false;
    QString message;
    QVERIFY(helperSchedule(3600u, &success, &message));
    QVERIFY2(success, qPrintable(message));
    QCOMPARE(armedTimers().size(), 1);

    // Simulate the F3 defect: the helper process loses all in-memory state
    // while the transient timer it created stays armed.
    QString restartLog;
    int rc = -1;
    QVERIFY2(runCommand(QStringLiteral("systemctl"),
                        {QStringLiteral("restart"), QStringLiteral("lumalarm-wake-helper.service")},
                        &restartLog, 60000, &rc),
             qPrintable(QStringLiteral("could not restart the helper: %1").arg(restartLog)));
    QCOMPARE(rc, 0);

    // The timer must have survived the restart.
    QCOMPARE(armedTimers().size(), 1);

    // GetStatus must not report idle for a wake that is still armed, and it
    // must still know the requested delay (recovered from the unit label).
    QString status;
    QVERIFY(helperStatus(&status));
    QVERIFY2(status.startsWith(QStringLiteral("scheduled")),
             qPrintable(QStringLiteral("helper reported '%1' for an armed wake after a "
                                       "restart")
                            .arg(status)));

    // Cancelling after the restart must actually remove the timer rather than
    // reporting a false success.
    QVERIFY(helperCancel(&success, &message));
    QVERIFY2(success, qPrintable(QStringLiteral("cancel after restart failed: %1").arg(message)));
    QVERIFY2(armedTimers().isEmpty(), "the timer survived cancellation after a restart");

    QVERIFY(helperStatus(&status));
    QCOMPARE(status, QStringLiteral("idle"));
}

void TestStg1Security::liveNonRootCallerIsDeniedWhileStatusStaysPublic()
{
    QString why;
    if (!liveHelperReady(&why))
        QSKIP(qPrintable(why));
    if (!commandExists(QStringLiteral("systemd-run")) || !commandExists(QStringLiteral("busctl")))
        QSKIP("systemd-run/busctl unavailable — cannot impersonate a non-root caller");

    // The caller is started by PID 1 as an unprivileged uid, so it has no
    // logind session. polkit then evaluates it under allow_inactive=no and
    // denies it outright — no challenge, no dialog, no dependence on which
    // authentication agent the desktop happens to run. The denial is
    // therefore deterministic while still exercising the real root helper,
    // the real caller identification and real polkit.
    const auto runAsNobody = [](const QStringList &methodArgs, QString *out, int *rc) {
        QStringList args{
            QStringLiteral("--scope"),
            QStringLiteral("--collect"),
            QStringLiteral("--uid=65534"),
            QStringLiteral("--gid=65534"),
            QStringLiteral("busctl"),
            QStringLiteral("call"),
            QStringLiteral("org.lumalarm.Helper"),
            QStringLiteral("/org/lumalarm/Helper"),
            QStringLiteral("org.lumalarm.Helper"),
        };
        args << methodArgs;
        return runCommand(QStringLiteral("systemd-run"), args, out, 60000, rc);
    };

    // Canary: the impersonation itself must work before anything is asserted.
    QString out;
    int rc = -1;
    if (!runAsNobody({QStringLiteral("GetStatus")}, &out, &rc) || rc != 0)
        QSKIP(qPrintable(
            QStringLiteral("could not launch a session-less non-root bus client: %1").arg(out)));

    // ScheduleWake: denied, and nothing may be created by the attempt.
    QVERIFY(runAsNobody({QStringLiteral("ScheduleWake"), QStringLiteral("u"), QStringLiteral("60")},
                        &out, &rc));
    QVERIFY2(rc == 0, qPrintable(QStringLiteral("busctl failed: %1").arg(out)));
    QVERIFY2(busctlReply(out).startsWith(QStringLiteral("false")),
             qPrintable(QStringLiteral("a non-root ScheduleWake was not denied: %1").arg(out)));
    QVERIFY2(armedTimers().isEmpty(), "a denied non-root ScheduleWake still created a wake timer");

    // CancelWake: denied too — cancelling is privileged as well.
    QVERIFY(runAsNobody({QStringLiteral("CancelWake")}, &out, &rc));
    QVERIFY2(rc == 0, qPrintable(QStringLiteral("busctl failed: %1").arg(out)));
    QVERIFY2(busctlReply(out).startsWith(QStringLiteral("false")),
             qPrintable(QStringLiteral("a non-root CancelWake was not denied: %1").arg(out)));

    // GetStatus: public read-only by design (F7) — the state word only.
    QVERIFY(runAsNobody({QStringLiteral("GetStatus")}, &out, &rc));
    QString status = busctlReply(out);
    if (status.startsWith(QLatin1Char('"')) && status.endsWith(QLatin1Char('"')))
        status = status.mid(1, status.size() - 2);
    QVERIFY2(status == QLatin1String("idle") || status.startsWith(QLatin1String("scheduled")),
             qPrintable(QStringLiteral("unexpected public status: %1").arg(status)));
    QVERIFY2(!status.contains(QLatin1Char('/')),
             qPrintable(QStringLiteral("GetStatus leaked filesystem detail: %1").arg(status)));
    QVERIFY2(!status.contains(QStringLiteral("error:")),
             qPrintable(QStringLiteral("GetStatus leaked the free-form error: %1").arg(status)));
}

QTEST_GUILESS_MAIN(TestStg1Security)
#include "tst_stg1security.moc"
