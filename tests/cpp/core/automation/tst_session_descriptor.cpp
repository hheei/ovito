// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Tests of the discovery half of the local protocol (Phase 2.6, deliverable 8): the descriptor a running workbench
// leaves behind for a client that wants to find it.
//
// AutomationSessionDescriptor is deliberately independent of the OVITO object model - it reads and writes one small
// JSON file and asks the operating system whether a process exists - so this suite runs without an Application
// instance, without an ambient task and without a GUI. Every case works inside a private discovery directory of its
// own (OVITO_AUTOMATION_SESSION_DIR), because the real one belongs to the running user.

#include <ovito/core/automation/AutomationContract.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace Ovito;

namespace {

/// Sets OVITO_AUTOMATION_SESSION_DIR for the duration of one test and restores the previous value afterwards.
class PrivateSessionDirectory
{
public:
    explicit PrivateSessionDirectory(const QString& path)
    {
        _previous = qgetenv("OVITO_AUTOMATION_SESSION_DIR");
        qputenv("OVITO_AUTOMATION_SESSION_DIR", path.toLocal8Bit());
    }
    ~PrivateSessionDirectory()
    {
        if(_previous.isEmpty())
            qunsetenv("OVITO_AUTOMATION_SESSION_DIR");
        else
            qputenv("OVITO_AUTOMATION_SESSION_DIR", _previous);
    }
    PrivateSessionDirectory(const PrivateSessionDirectory&) = delete;
    PrivateSessionDirectory& operator=(const PrivateSessionDirectory&) = delete;

private:
    QByteArray _previous;
};

/// Writes a file of arbitrary content into the session directory.
bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(bytes) == bytes.size();
}

/// A process ID no process can have: the kernel rejects anything above the configured maximum.
qint64 deadProcessId()
{
    return 0x7FFFFFF0;
}

}   // namespace

class AutomationSessionDescriptorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // The value
    // -----------------------------------------------------------------------

    void allocate_new_session_gives_a_usable_name()
    {
        const AutomationSessionDescriptor first = AutomationSessionDescriptor::allocateNewSession();
        const AutomationSessionDescriptor second = AutomationSessionDescriptor::allocateNewSession();

        // Two sessions of the same process differ, and both are valid and short enough for a socket path.
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        QVERIFY(first.sessionId() != second.sessionId());

        // The session belongs to this process, which is how a client decides staleness later.
        QCOMPARE(first.processId(), QCoreApplication::applicationPid());
        QVERIFY(first.isProcessAlive());
        QVERIFY(!first.isStale());

        // The endpoint is what gets connected to, and it is an absolute socket path inside the session directory: a
        // socket named relatively would be created in the system's temporary directory, which every user can write to.
        QCOMPARE(first.endpoint(), AutomationSessionDescriptor::directory() + u'/' + first.sessionId());
        QVERIFY(first.endpoint().startsWith(u'/'));
        QVERIFY(first.endpoint().size() <= 104);
        QVERIFY(first.startedAt().isValid());

        // The contract version this build speaks is recorded, so a client can refuse a session of another major
        // protocol instead of guessing what its fields mean.
        QCOMPARE(first.contractVersion(), AutomationContract::version());

        // A prefix that cannot be part of a name is replaced rather than written into a path.
        const AutomationSessionDescriptor odd = AutomationSessionDescriptor::allocateNewSession(QStringLiteral("../etc"));
        QVERIFY(odd.isValid());
        QVERIFY(!odd.sessionId().contains(u'/'));
        QVERIFY(odd.sessionId().startsWith(QStringLiteral("ovito-")));
    }

    void descriptor_round_trips()
    {
        const AutomationSessionDescriptor original = AutomationSessionDescriptor::allocateNewSession();
        const QVariantMap json = original.toJson();

        QCOMPARE(json.value(QStringLiteral("discoveryVersion")).toInt(), AutomationSessionDescriptor::discoveryVersion);
        QCOMPARE(json.value(QStringLiteral("sessionId")).toString(), original.sessionId());
        QCOMPARE(json.value(QStringLiteral("processId")).toLongLong(), original.processId());
        QCOMPARE(json.value(QStringLiteral("endpoint")).toString(), original.endpoint());

        const std::optional<AutomationSessionDescriptor> parsed = AutomationSessionDescriptor::fromJson(json);
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->sessionId(), original.sessionId());
        QCOMPARE(parsed->processId(), original.processId());
        QCOMPARE(parsed->endpoint(), original.endpoint());
        QCOMPARE(parsed->startedAt(), original.startedAt());
        QCOMPARE(parsed->contractVersion(), original.contractVersion());
        QVERIFY(parsed->isValid());
    }

    void descriptor_rejects_what_is_not_one()
    {
        const QVariantMap valid = AutomationSessionDescriptor::allocateNewSession().toJson();

        QString error;
        // An empty map is not a descriptor.
        QVERIFY(!AutomationSessionDescriptor::fromJson({}, &error).has_value());
        QVERIFY(!error.isEmpty());

        // A missing session, a missing or nonsensical process ID and a missing endpoint are each refused, and the
        // reason names the field rather than only the file.
        QVariantMap json = valid;
        json.remove(QStringLiteral("sessionId"));
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("session")));

        json = valid;
        json.insert(QStringLiteral("processId"), QStringLiteral("not a number"));
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("process")));

        json = valid;
        json.insert(QStringLiteral("processId"), 0);
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());

        json = valid;
        json.insert(QStringLiteral("endpoint"), QStringLiteral("../escape"));
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("endpoint")));

        // A relative path would resolve differently for the session and for the client, and an endpoint longer than the
        // socket name limits would be truncated by the operating system.
        json = valid;
        json.insert(QStringLiteral("endpoint"), QStringLiteral("relative/socket"));
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());
        json = valid;
        json.insert(QStringLiteral("endpoint"), QStringLiteral("/") + QString(120, u'a'));
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());

        json = valid;
        json.insert(QStringLiteral("sessionId"), QString());
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());

        // A descriptor of another discovery version is refused as a version problem, not as a broken file.
        json = valid;
        json.insert(QStringLiteral("discoveryVersion"), AutomationSessionDescriptor::discoveryVersion + 1);
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("discovery version")));

        // A session of another *major* contract version is a real session this build cannot talk to. A minor difference
        // is compatible by the contract's own rule, so that one is accepted.
        json = valid;
        json.insert(QStringLiteral("contractVersion"), QStringLiteral("9.0"));
        QVERIFY(!AutomationSessionDescriptor::fromJson(json, &error).has_value());
        QVERIFY(error.contains(AutomationContract::version()));

        const QString major = AutomationContract::version().section(u'.', 0, 0);
        const QString futureMinor = major + QStringLiteral(".99");
        json = valid;
        json.insert(QStringLiteral("contractVersion"), futureMinor);
        QVERIFY(AutomationSessionDescriptor::fromJson(json, &error).has_value());
    }

    // -----------------------------------------------------------------------
    // The file
    // -----------------------------------------------------------------------

    void descriptor_is_written_with_owner_only_permissions()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString scope = temporaryDirectory.filePath(QStringLiteral("sessions"));
        PrivateSessionDirectory sessionDirectory(scope);
        QCOMPARE(AutomationSessionDescriptor::directory(), scope);

        const AutomationSessionDescriptor descriptor = AutomationSessionDescriptor::allocateNewSession();
        QString error;
        QVERIFY(descriptor.writeToDisk(&error));
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QString path = AutomationSessionDescriptor::filePath(descriptor.sessionId());
        QCOMPARE(path, scope + u'/' + descriptor.sessionId() + QString::fromLatin1(AutomationSessionDescriptor::fileSuffix));
        QVERIFY(QFile::exists(path));

        // The directory names this user's sessions and the file names one of them; nobody else may read either.
#if defined(Q_OS_UNIX)
        const QFile::Permissions filePermissions = QFile::permissions(path);
        QVERIFY(filePermissions.testFlag(QFile::ReadOwner));
        QVERIFY(!filePermissions.testFlag(QFile::ReadGroup));
        QVERIFY(!filePermissions.testFlag(QFile::ReadOther));
        const QFile::Permissions directoryPermissions = QFile::permissions(scope);
        QVERIFY(directoryPermissions.testFlag(QFile::ReadOwner));
        QVERIFY(!directoryPermissions.testFlag(QFile::ReadOther));
#endif

        // What is on disk is the descriptor and nothing else - no temporary sibling left behind.
        const QStringList entries = QDir(scope).entryList(QDir::Files);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.front(), descriptor.sessionId() + QString::fromLatin1(AutomationSessionDescriptor::fileSuffix));

        // What a reader finds is the same session.
        QStringList problems;
        const QVector<AutomationSessionDescriptor> found = AutomationSessionDescriptor::discover(&problems);
        QCOMPARE(found.size(), 1);
        QVERIFY(problems.isEmpty());
        QCOMPARE(found.front().sessionId(), descriptor.sessionId());
        QCOMPARE(found.front().endpoint(), descriptor.endpoint());
        QCOMPARE(found.front().processId(), descriptor.processId());

        // Removing it is idempotent, because a client that cleans up a stale descriptor may race a session that is
        // shutting down.
        QVERIFY(descriptor.removeFromDisk());
        QVERIFY(!QFile::exists(path));
        QVERIFY(descriptor.removeFromDisk());
        QCOMPARE(AutomationSessionDescriptor::discover(&problems).size(), 0);
    }

    void write_is_refused_for_an_unusable_descriptor()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        PrivateSessionDirectory sessionDirectory(temporaryDirectory.filePath(QStringLiteral("sessions")));

        // A descriptor without a process ID would invite a client to treat it as stale forever.
        const AutomationSessionDescriptor incomplete(QStringLiteral("ovito-1-deadbeef"), 0, QStringLiteral("ovito-1-deadbeef"));
        QVERIFY(!incomplete.isValid());
        QString error;
        QVERIFY(!incomplete.writeToDisk(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(QDir(AutomationSessionDescriptor::directory()).entryList(QDir::Files).isEmpty());
    }

    // -----------------------------------------------------------------------
    // Discovery
    // -----------------------------------------------------------------------

    void discovery_reports_foreign_files_and_respects_its_limit()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString scope = temporaryDirectory.filePath(QStringLiteral("sessions"));
        PrivateSessionDirectory sessionDirectory(scope);

        // Two real sessions, one of them older.
        const AutomationSessionDescriptor older = AutomationSessionDescriptor::allocateNewSession();
        QVERIFY(older.writeToDisk());
        const AutomationSessionDescriptor newer(
            AutomationSessionDescriptor::allocateNewSession().sessionId(), QCoreApplication::applicationPid(),
            AutomationSessionDescriptor::allocateNewSession().endpoint(), QDateTime::currentDateTime().addSecs(5));
        QVERIFY(newer.writeToDisk());

        // A file that is not JSON, and a JSON file that is not a descriptor: both stay out of the session list and are
        // reported by name, so a client can tell "no session" from "something else is in my directory".
        QVERIFY(writeFile(scope + u"/garbage" + QString::fromLatin1(AutomationSessionDescriptor::fileSuffix), "not json at all\n"));
        QVERIFY(writeFile(scope + u"/foreign" + QString::fromLatin1(AutomationSessionDescriptor::fileSuffix), "{\"hello\":\"world\"}\n"));
        // A file without the suffix is not looked at at all.
        QVERIFY(writeFile(scope + u"/notes.txt", "hello\n"));

        QStringList problems;
        const QVector<AutomationSessionDescriptor> found = AutomationSessionDescriptor::discover(&problems);
        QCOMPARE(found.size(), 2);
        QCOMPARE(problems.size(), 2);
        // Newest first: a bare "connect" means the session the user started last.
        QCOMPARE(found.front().sessionId(), newer.sessionId());
        QCOMPARE(found.back().sessionId(), older.sessionId());

        // The limit bounds the work a client does in a directory that holds many sessions.
        QCOMPARE(AutomationSessionDescriptor::discover(nullptr, 1).size(), 1);

        // A directory that does not exist yet is an empty discovery scope and not an error.
        QVERIFY(QDir(scope).removeRecursively());
        QStringList missingProblems;
        QVERIFY(AutomationSessionDescriptor::discover(&missingProblems).isEmpty());
        QVERIFY(missingProblems.isEmpty());
    }

    void a_session_whose_process_is_gone_is_reported_as_stale()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        PrivateSessionDirectory sessionDirectory(temporaryDirectory.filePath(QStringLiteral("sessions")));

        // A descriptor of a process that cannot exist: a workbench that was killed has no chance to clean up after
        // itself, which is the whole reason a client has to be able to ask.
        const AutomationSessionDescriptor leftover(QStringLiteral("ovito-424242-cafe0001"), deadProcessId(),
                                                   QStringLiteral("ovito-424242-cafe0001"));
        QVERIFY(leftover.isValid());
        QVERIFY(leftover.writeToDisk());

        // And the live one belongs to this process.
        const AutomationSessionDescriptor live = AutomationSessionDescriptor::allocateNewSession();
        QVERIFY(live.writeToDisk());

        QVERIFY(leftover.isStale());
        QVERIFY(!leftover.isProcessAlive());
        QVERIFY(!live.isStale());

        // Discovery reports both, with the truth about each: a client decides what to do with a stale entry (the spike's
        // "list sessions" prunes it, a production client may want to tell its user about it first).
        const QVector<AutomationSessionDescriptor> found = AutomationSessionDescriptor::discover();
        QCOMPARE(found.size(), 2);
        int staleCount = 0;
        for(const AutomationSessionDescriptor& descriptor : found)
            staleCount += descriptor.isStale() ? 1 : 0;
        QCOMPARE(staleCount, 1);
    }

    void the_discovery_scope_can_be_relocated()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString scope = temporaryDirectory.filePath(QStringLiteral("relocated"));
        {
            PrivateSessionDirectory sessionDirectory(scope);
            QCOMPARE(AutomationSessionDescriptor::directory(), scope);
            QVERIFY(AutomationSessionDescriptor::allocateNewSession().writeToDisk());
            QCOMPARE(AutomationSessionDescriptor::discover().size(), 1);
        }
        // Outside the override the real scope is used again, and it is not the temporary one.
        QVERIFY(AutomationSessionDescriptor::directory() != scope);
        QVERIFY(!AutomationSessionDescriptor::directory().isEmpty());
    }
};

QTEST_MAIN(AutomationSessionDescriptorTest)
#include "tst_session_descriptor.moc"
