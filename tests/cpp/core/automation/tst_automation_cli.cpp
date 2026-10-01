// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <QtTest>

#include <ovito/core/automation/AutomationCommandLine.h>
#include <ovito/core/automation/AutomationContract.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

using namespace Ovito;

namespace {

/**
 * \brief A private discovery directory for one test function.
 *
 * The command line finds sessions through the same convention as every other client, so a test that wants to know
 * exactly what it will find has to own the directory. Setting and restoring the environment variable is what makes the
 * checks independent of the sessions a developer happens to have running.
 */
class PrivateSessionDirectory
{
public:
    PrivateSessionDirectory()
    {
        _temporary = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/ovito-cli-test-XXXXXX"));
        QVERIFY(_temporary->isValid());
        _previous = qgetenv("OVITO_AUTOMATION_SESSION_DIR");
        qputenv("OVITO_AUTOMATION_SESSION_DIR", _temporary->path().toUtf8());
    }

    ~PrivateSessionDirectory()
    {
        if(_previous.isEmpty())
            qunsetenv("OVITO_AUTOMATION_SESSION_DIR");
        else
            qputenv("OVITO_AUTOMATION_SESSION_DIR", _previous);
    }

    /// The directory the descriptors of this test live in.
    QString path() const { return _temporary->path(); }

private:
    std::unique_ptr<QTemporaryDir> _temporary;
    QByteArray _previous;
};

/// The `ovito` binary this build produced, or an empty string when this build has no executable to talk to.
///
/// The tests are configured before the product executable exists, so its directory is not passed to them; instead this
/// walks up from the build directory of this test (which the test framework defines) to the build tree and looks for
/// the executable where every non-PyPI build puts it. A build without one - a frontend-only preset, a Python wheel,
/// a library-only build - makes the suite skip itself rather than fail.
QString ovitoBinary()
{
    // Every layout the build can produce: a Linux or Conda build puts the executable in bin/, a macOS build inside
    // the application bundle, and a plain Windows build (*not* a Conda one) directly into the build directory, because
    // OVITO_RELATIVE_BINARY_DIRECTORY is "." there - which is why the walk has to look at the directory itself and not
    // only for bin/ below it. Missing that candidate made this suite fail on the Windows runner for want of a binary
    // that was sitting right there (see UI_TEST_ENV.md 6.2).
    const QStringList candidates = {
        QStringLiteral("ovito"),
        QStringLiteral("ovito.exe"),
        QStringLiteral("bin/ovito"),
        QStringLiteral("bin/ovito.exe"),
        QStringLiteral("bin/Ovito.app/Contents/MacOS/ovito"),
        QStringLiteral("Ovito.app/Contents/MacOS/ovito")
    };
    QDir directory(QStringLiteral(QT_TESTCASE_BUILDDIR));
    while(true) {
        for(const QString& candidate : candidates) {
            const QString path = directory.absoluteFilePath(candidate);
            if(QFile::exists(path) && QFileInfo(path).isExecutable())
                return path;
        }
        if(directory.isRoot() || !directory.cdUp())
            break;
    }
    return {};
}

/// Runs one invocation of the command line and returns its exit code with the standard output and error it produced.
struct Invocation {
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
    QJsonObject json() const { return QJsonDocument::fromJson(standardOutput.toUtf8()).object(); }
};

Invocation runCommandLine(const QString& binary, const QStringList& arguments, const QByteArray& sessionDirectory)
{
    QProcess process;
    process.setProgram(binary);
    process.setArguments(arguments);
    // The client inherits the discovery scope of the test, so that the two sides agree on which sessions exist.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("OVITO_AUTOMATION_SESSION_DIR"), QString::fromUtf8(sessionDirectory));
    process.setProcessEnvironment(environment);
    process.start();
    if(!process.waitForStarted(20000)) {
        Invocation result;
        result.exitCode = -1;
        result.standardError = QStringLiteral("the command line did not start: %1").arg(process.errorString());
        return result;
    }
    if(!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished(5000);
        Invocation result;
        result.exitCode = -1;
        result.standardError = QStringLiteral("the command line did not finish in time");
        return result;
    }
    Invocation result;
    result.exitCode = process.exitCode();
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.standardError = QString::fromUtf8(process.readAllStandardError());
    return result;
}

}   // End of anonymous namespace

/**
 * \brief Tests the `ovito --automation` command line, the read-only client of a running session (Phase 3, deliverable 7).
 *
 * The command line is a client, so the interesting cases are the ones a client lives through before it ever reaches a
 * session: an empty discovery scope, a descriptor whose process is gone, a verb that does not exist, and an invocation
 * that is missing the argument of its verb. Each of them is expected to answer *and* to say why it could not carry the
 * request out, because a script reads the exit code and the JSON, not the prose.
 *
 * The one thing this suite cannot check on its own is the successful exchange with a live workbench: that needs a
 * process which actually serves a session, and it is covered by the QML frontend's `--qml-automation-check`, which
 * starts a serving workbench and runs this same command line against it.
 */
class AutomationCommandLineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        _binary = ovitoBinary();
        if(_binary.isEmpty()) {
            // A build that does not produce the application (a frontend-only preset, a library build) has nothing to
            // run, and skipping is honest there. A job that wrote the binary must not silently lose this coverage
            // instead, so it asks for the failure: OVITO_REQUIRE_TEST_BINARY is set by the CI jobs that build it.
            if(qEnvironmentVariableIsSet("OVITO_REQUIRE_TEST_BINARY"))
                QFAIL("This build produced no ovito executable, although the test was told to require one.");
            QSKIP("This build produced no ovito executable to run the command line from.");
        }
    }

    /// The verbs the help text offers, and the capabilities a client of the command line asks for.
    void vocabulary_is_read_only()
    {
        QCOMPARE(AutomationCommandLine::verbs(), QStringList({QStringLiteral("list"), QStringLiteral("status"), QStringLiteral("describe"), QStringLiteral("snapshot"), QStringLiteral("events")}));

        // A client that connects to look around must not be able to change anything, run Python or write a file, even
        // if the session it talks to would allow it: it asks for the read capabilities and for nothing else.
        const QStringList requested = AutomationCommandLine::requestedCapabilities();
        QCOMPARE(requested.size(), 4);
        for(const QString& capability : requested) {
            const std::optional<AutomationContract::Capability> parsed = AutomationContract::capabilityFromName(capability);
            QVERIFY2(parsed.has_value(), qPrintable(QStringLiteral("the command line asked for the unknown capability %1").arg(capability)));
            QVERIFY2(AutomationContract::isReadCapability(*parsed),
                     qPrintable(QStringLiteral("the command line asked for the non-read capability %1").arg(capability)));
        }
        QVERIFY(requested.contains(QStringLiteral("session.read")));
        QVERIFY(requested.contains(QStringLiteral("scene.read")));
        QVERIFY(requested.contains(QStringLiteral("selection.read")));
        QVERIFY(requested.contains(QStringLiteral("file.read")));
    }

    /// An empty discovery scope is not an error for a listing, and no session at all is one for every other verb.
    void without_a_session_it_explains_itself()
    {
        PrivateSessionDirectory scope;

        Invocation listing = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("list"), QStringLiteral("--json")}, scope.path().toUtf8());
        QCOMPARE(listing.exitCode, 0);
        const QJsonObject listed = listing.json();
        QCOMPARE(listed.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(listed.value(QStringLiteral("sessions")).toArray().size(), 0);
        // The human-readable form of the same answer says the same thing in one line.
        Invocation human = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("list")}, scope.path().toUtf8());
        QCOMPARE(human.exitCode, 0);
        QVERIFY(human.standardOutput.contains(QStringLiteral("no running session")));

        for(const QString& verb : {QStringLiteral("status"), QStringLiteral("snapshot"), QStringLiteral("events")}) {
            Invocation invocation = runCommandLine(_binary, {QStringLiteral("--automation"), verb, QStringLiteral("--json")}, scope.path().toUtf8());
            QCOMPARE(invocation.exitCode, 2);
            const QJsonObject answer = invocation.json();
            QCOMPARE(answer.value(QStringLiteral("ok")).toBool(), false);
            QCOMPARE(answer.value(QStringLiteral("verb")).toString(), verb);
            QCOMPARE(answer.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString(), QStringLiteral("no_session"));
        }

        // The command line never invents an object: asking about one without a session is the same answer.
        Invocation describe = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("describe"), QStringLiteral("viewport:v1"), QStringLiteral("--json")}, scope.path().toUtf8());
        QCOMPARE(describe.exitCode, 2);
        QCOMPARE(describe.json().value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString(), QStringLiteral("no_session"));
    }

    /// A wrong invocation is answered without a session being looked for, because there is nothing it could answer.
    void bad_requests_are_refused_before_anything_is_reached()
    {
        PrivateSessionDirectory scope;

        Invocation unknownVerb = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("recolor")}, scope.path().toUtf8());
        QCOMPARE(unknownVerb.exitCode, 2);
        QVERIFY(unknownVerb.standardError.contains(QStringLiteral("is not an automation verb")));
        // The message names what this build does know, so that a script can be fixed without reading a manual.
        for(const QString& verb : AutomationCommandLine::verbs())
            QVERIFY(unknownVerb.standardError.contains(verb));

        Invocation missingArgument = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("describe")}, scope.path().toUtf8());
        QCOMPARE(missingArgument.exitCode, 2);
        QVERIFY(missingArgument.standardError.contains(QStringLiteral("describe <object-id>")));

        // No verb at all: the usage line is the answer, and it is an exit code of its own.
        Invocation noVerb = runCommandLine(_binary, {QStringLiteral("--automation")}, scope.path().toUtf8());
        QVERIFY(noVerb.exitCode != 0);
    }

    /// A descriptor whose process is gone is a leftover: reported, not connected to, and not removed either.
    void a_stale_descriptor_is_reported_and_left_alone()
    {
        PrivateSessionDirectory scope;

        // A session that announces a process which cannot exist and a socket that nobody listens on. Its contract
        // version is the one this build speaks, so the descriptor is accepted as a session and fails on the connection.
        AutomationSessionDescriptor stale(QStringLiteral("ovito-999999-cli"), 999999,
                                          QDir(scope.path()).absoluteFilePath(QStringLiteral("ovito-999999-cli.sock")),
                                          QDateTime::currentDateTime());
        QString error;
        QVERIFY2(stale.writeToDisk(&error), qPrintable(error));

        // The listing reports the unusable file separately instead of silently skipping it.
        Invocation listing = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("list"), QStringLiteral("--json")}, scope.path().toUtf8());
        QCOMPARE(listing.exitCode, 0);
        const QJsonObject listed = listing.json();
        QVERIFY(listed.value(QStringLiteral("sessions")).toArray().size() + listed.value(QStringLiteral("problems")).toArray().size() >= 1);

        // Talking to it cannot work, and the answer says so rather than pretending there is no session at all: the
        // difference is what tells a user whether their workbench died.
        Invocation status = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("status"), QStringLiteral("--json")}, scope.path().toUtf8());
        QCOMPARE(status.exitCode, 2);
        const QJsonObject answer = status.json();
        QCOMPARE(answer.value(QStringLiteral("ok")).toBool(), false);
        QVERIFY(answer.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString().contains(QStringLiteral("does not answer")));

        // The client does not clean up after another process: deleting a discovery entry is the decision of the process
        // that owns it (audit decision D50), so the file is still there afterwards.
        QVERIFY(QFile::exists(QDir(scope.path()).absoluteFilePath(QStringLiteral("ovito-999999-cli.session.json"))));
    }

    /// The JSON form is one object per invocation, so that a script can read the answer of a verb without a parser.
    void json_mode_prints_one_object()
    {
        PrivateSessionDirectory scope;
        Invocation listing = runCommandLine(_binary, {QStringLiteral("--automation"), QStringLiteral("list"), QStringLiteral("--json")}, scope.path().toUtf8());
        QCOMPARE(listing.exitCode, 0);
        // One line, i.e. one object: a caller can read it with a line-oriented reader and does not need a parser for
        // pretty-printed JSON.
        QCOMPARE(listing.standardOutput.trimmed().split(QLatin1Char('\n')).size(), 1);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(listing.standardOutput.trimmed().toUtf8(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());
    }

private:

    /// The executable of this build, resolved in initTestCase().
    QString _binary;
};

QTEST_MAIN(AutomationCommandLineTest)
#include "tst_automation_cli.moc"
