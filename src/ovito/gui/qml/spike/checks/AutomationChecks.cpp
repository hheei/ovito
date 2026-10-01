// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/spike/SpikeHarness.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/gui/base/app/WorkbenchUI.h>
#include <ovito/core/automation/AutomationCommandLine.h>
#include <ovito/core/automation/AutomationContract.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>
#include <ovito/core/automation/transport/AutomationLocalClient.h>
#include <ovito/core/automation/transport/AutomationLocalEndpoint.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace Ovito::Spike {

namespace {

/**
 * \brief The state of the check as it moves from one phase to the next.
 *
 * The phases are chained through delayed continuations, so the state has to outlive the function that started them; it
 * also collects the answer of the command line process, which arrives asynchronously while the event loop is running.
 */
struct AutomationCheckState
{
    /// The session directory this check owns, so that it neither sees nor disturbs the sessions of the user.
    QString sessionDirectory;

    /// The command line process of the running phase, if one is running.
    QPointer<QProcess> process;

    /// What the last command line invocation answered.
    QJsonObject answer;
    int exitCode = -1;
    QString standardError;

    /// The viewport ID the check asked about, taken from the snapshot the command line reported.
    QString viewportId;

    /// The continuation of the check, called when every phase has run.
    std::function<void()> continuation;

    /// Reports a failed check and ends the phase without stopping the whole run: the remaining phases are still worth
    /// their evidence, and the harness counts the failures.
    void fail(const QString& message) const
    {
        qWarning("VERIFY_FAILED %s", qPrintable(message));
    }
};

/// The shortest writable temp base this platform has.
///
/// A session's socket path may be 104 characters on macOS and the platform's own temporary directory can spend most of
/// that on its own: on macOS it is `/var/folders/<2>/<30>/T`, which leaves no room for a session directory plus a
/// session ID (the workbench then refuses to serve, correctly, and this check used to fail there for that reason -
/// see UI_TEST_ENV.md 6.2). `/tmp` is short on every UNIX (macOS resolves it to `/private/tmp`).
QString shortTempBase()
{
#if defined(Q_OS_UNIX)
    const QFileInfo base(QStringLiteral("/tmp"));
    if(base.isDir() && base.isWritable())
        return QStringLiteral("/tmp");
#endif
    return QDir::tempPath();
}

/// The `ovito` executable of this build, next to the spike binary, or an empty string when there is none.
QString ovitoBinary()
{
    const QDir directory(QCoreApplication::applicationDirPath());
    for(const QString& name : { QStringLiteral("ovito"), QStringLiteral("ovito.exe"), QStringLiteral("Ovito") }) {
        const QString path = directory.absoluteFilePath(name);
        if(QFileInfo(path).isExecutable())
            return path;
    }
    return {};
}

/// Starts the command line of this build with the given arguments against the session scope of the check.
void runCommandLine(QmlMainWindowUI* ui, const std::shared_ptr<AutomationCheckState>& state, const QStringList& arguments,
                    std::function<void(bool started)> done)
{
    const QString binary = ovitoBinary();
    if(binary.isEmpty()) {
        state->fail(QStringLiteral("the 'ovito' executable of this build was not found next to the spike binary"));
        done(false);
        return;
    }
    QProcess* process = new QProcess(ui->view());
    // The child must see the same session directory as this process, otherwise it would look for the sessions of the
    // user instead of the one this check serves.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("OVITO_AUTOMATION_SESSION_DIR"), state->sessionDirectory);
    process->setProcessEnvironment(environment);
    process->setProgram(binary);
    process->setArguments(arguments);
    state->process = process;
    state->answer = {};
    state->exitCode = -1;
    state->standardError.clear();
    process->start();
    // The answer arrives while the event loop runs; the caller polls for the process to finish rather than blocking
    // here, because the endpoint that answers it lives in this process and its loop must stay responsive.
    QTimer::singleShot(0, ui->view(), [state, done, process]() {
        done(process->state() != QProcess::NotRunning || process->waitForStarted(2000));
    });
}

/// Takes what a finished command line process answered.
void collectAnswer(const std::shared_ptr<AutomationCheckState>& state)
{
    if(!state->process)
        return;
    state->exitCode = state->process->exitCode();
    state->standardError = QString::fromUtf8(state->process->readAllStandardError());
    const QByteArray output = state->process->readAllStandardOutput();
    state->answer = QJsonDocument::fromJson(output.trimmed()).object();
    state->process.clear();
}

/// The capabilities the contract calls "work", i.e. the ones a read-only client must not have been granted.
QStringList workCapabilities()
{
    QStringList names;
    for(int value = 0; value <= static_cast<int>(AutomationContract::Capability::ProcessExecute); value++) {
        const auto capability = static_cast<AutomationContract::Capability>(value);
        if(!AutomationContract::isReadCapability(capability))
            names.push_back(AutomationContract::capabilityName(capability));
    }
    return names;
}

}   // End of anonymous namespace

/******************************************************************************
* Verifies the automation foundation of the frontend: a workbench serves its session only when it was asked to, the
* command line of this build finds it, and the read-only client rule holds end to end.
*
* This is the check of Phase 3 slice S4 (deliverable 7). It is the one check that exercises the *whole* path a machine
* client takes - the session directory convention, the local socket, the contract operations, the capability grant and
* the command line - and it does so with two processes, because a client that blocks the event loop of the workbench it
* talks to could not be answered (this is also why the command line is started as a child process rather than called in
* process).
*
* The session directory is private to the check: it is put into the environment before the endpoint starts, so the
* check can assert exactly which sessions exist - "none" before the workbench serves, "one" while it does and "none"
* again after it stopped, which is the teardown half of open item O18.
******************************************************************************/
void runAutomationTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // The phases this check walks through, in the order of the numbered sections below.
    declareCheckPhases({ "before serving", "serving", "status", "snapshot", "describe", "stopped serving" });

    auto state = std::make_shared<AutomationCheckState>();
    state->continuation = std::move(continuation);
    // A name short enough to leave room for a session ID below the 104-character socket-path limit.
    state->sessionDirectory = shortTempBase() + QStringLiteral("/ovito-qml-check-%1").arg(QCoreApplication::applicationPid());
    QDir().mkpath(state->sessionDirectory);
    qputenv("OVITO_AUTOMATION_SESSION_DIR", state->sessionDirectory.toUtf8());

    qInfo("AUTOMATION_TEST session directory %s", qPrintable(state->sessionDirectory));

    // ---------------------------------------------------------------- 1. a workbench is invisible until it is asked to serve
    {
        QStringList problems;
        const QVector<AutomationSessionDescriptor> sessions = AutomationLocalClient::discover(&problems);
        qInfo("AUTOMATION_TEST sessions before serving: %lld (%lld unusable files)",
              static_cast<long long>(sessions.size()), static_cast<long long>(problems.size()));
        if(!sessions.empty())
            state->fail(QStringLiteral("a workbench that was not asked to serve a session is discoverable"));
    }
    reportCheckPhase("before serving");

    // ---------------------------------------------------------------- 2. serving publishes exactly one session
    AutomationLocalEndpoint* endpoint = nullptr;
    {
        GuiTaskScope scope(*ui);
        endpoint = ui->startAutomationServer();
    }
    if(!endpoint) {
        state->fail(QStringLiteral("the workbench could not serve its session"));
        state->continuation();
        return;
    }
    {
        QStringList problems;
        const QVector<AutomationSessionDescriptor> sessions = AutomationLocalClient::discover(&problems);
        qInfo("AUTOMATION_TEST sessions while serving: %lld, endpoint %s",
              static_cast<long long>(sessions.size()), qPrintable(endpoint->descriptor().endpoint()));
        if(sessions.size() != 1)
            state->fail(QStringLiteral("a serving workbench published %1 sessions instead of one").arg(sessions.size()));
        else if(sessions.front().processId() != QCoreApplication::applicationPid())
            state->fail(QStringLiteral("the published session belongs to another process"));
        // The endpoint is a socket inside the owner-only directory, never a TCP port.
        if(!sessions.empty() && !sessions.front().endpoint().startsWith(state->sessionDirectory))
            state->fail(QStringLiteral("the session's endpoint lies outside the session directory"));
    }

    reportCheckPhase("serving");

    const quint64 revisionBefore = ui->automationSession()->revision();

    // ---------------------------------------------------------------- 3. the command line of this build asks it
    runCommandLine(ui, state, { QStringLiteral("--automation"), QStringLiteral("status"), QStringLiteral("--json") }, [ui, state, revisionBefore](bool started) {
        if(!started) {
            state->continuation();
            return;
        }
        pollUntil(ui, 50, 30000, [state]() { return !state->process || state->process->state() == QProcess::NotRunning; }, [ui, state, revisionBefore](bool finished) {
            if(!finished) {
                state->fail(QStringLiteral("the automation command line did not finish"));
                if(state->process)
                    state->process->kill();
                state->continuation();
                return;
            }
            collectAnswer(state);

            const QJsonObject& answer = state->answer;
            const QJsonObject session = answer.value(QStringLiteral("session")).toObject();
            qInfo("AUTOMATION_TEST status: exit %d, session %s, revision %lld, scene nodes %d, viewports %d",
                  state->exitCode,
                  qPrintable(QString::number(answer.value(QStringLiteral("ok")).toBool())),
                  static_cast<long long>(session.value(QStringLiteral("revision")).toVariant().toULongLong()),
                  session.value(QStringLiteral("sceneNodeCount")).toInt(),
                  static_cast<int>(answer.value(QStringLiteral("viewports")).toObject().value(QStringLiteral("viewports")).toArray().size()));

            if(state->exitCode != 0)
                state->fail(QStringLiteral("the command line reported exit code %1: %2").arg(state->exitCode).arg(state->standardError.trimmed()));
            if(!answer.value(QStringLiteral("ok")).toBool())
                state->fail(QStringLiteral("the command line could not read the session: %1").arg(QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact))));
            // The answer describes this session and not another one: the revision it reports is the one the workbench
            // is at, which is what a client bases its next request on.
            if(session.value(QStringLiteral("revision")).toVariant().toULongLong() != revisionBefore)
                state->fail(QStringLiteral("the session revision the command line reported is not the one of this workbench"));
            if(session.value(QStringLiteral("hasDataSet")).toBool() != true)
                state->fail(QStringLiteral("the command line reported a session without a data set"));
            if(answer.value(QStringLiteral("viewports")).toObject().value(QStringLiteral("viewports")).toArray().size() != 4)
                state->fail(QStringLiteral("the session did not report its four viewports"));

            // The read-only rule, end to end: the client asked for the read capabilities, the workbench granted exactly
            // those, and none of the capabilities that permit a change is in the answer.
            QStringList granted, refused;
            for(const QJsonValue& entry : answer.value(QStringLiteral("grantedCapabilities")).toArray())
                granted.push_back(entry.toString());
            for(const QJsonValue& entry : answer.value(QStringLiteral("refusedCapabilities")).toArray())
                refused.push_back(entry.toString());
            qInfo("AUTOMATION_TEST granted %s%s",
                  qPrintable(granted.join(QStringLiteral(", "))),
                  refused.isEmpty() ? "" : qPrintable(QStringLiteral(", refused %1").arg(refused.join(QStringLiteral(", ")))));
            for(const QString& capability : workCapabilities()) {
                if(granted.contains(capability))
                    state->fail(QStringLiteral("the read-only session granted the capability %1").arg(capability));
            }
            if(!refused.isEmpty())
                state->fail(QStringLiteral("the session refused capabilities the read-only client asked for: %1").arg(refused.join(QStringLiteral(", "))));

            reportCheckPhase("status");

            // ------------------------------------------------------------ 4. the snapshot composes the viewport and selection state
            runCommandLine(ui, state, { QStringLiteral("--automation"), QStringLiteral("snapshot"), QStringLiteral("--json") }, [ui, state](bool started) {
                if(!started) {
                    state->continuation();
                    return;
                }
                pollUntil(ui, 50, 30000, [state]() { return !state->process || state->process->state() == QProcess::NotRunning; }, [ui, state](bool finished) {
                    if(!finished) {
                        state->fail(QStringLiteral("the snapshot command did not finish"));
                        if(state->process)
                            state->process->kill();
                        state->continuation();
                        return;
                    }
                    collectAnswer(state);
                    const QJsonObject snapshot = state->answer.value(QStringLiteral("snapshot")).toObject();
                    const QJsonArray viewports = snapshot.value(QStringLiteral("viewports")).toObject().value(QStringLiteral("viewports")).toArray();
                    qInfo("AUTOMATION_TEST snapshot: exit %d, %lld viewports, %lld events, scene %lld nodes",
                          state->exitCode, static_cast<long long>(viewports.size()),
                          static_cast<long long>(snapshot.value(QStringLiteral("events")).toObject().value(QStringLiteral("events")).toArray().size()),
                          static_cast<long long>(snapshot.value(QStringLiteral("scene")).toObject().value(QStringLiteral("nodeCount")).toInt()));
                    if(state->exitCode != 0)
                        state->fail(QStringLiteral("the snapshot command reported exit code %1: %2").arg(state->exitCode).arg(state->standardError.trimmed()));
                    if(snapshot.isEmpty())
                        state->fail(QStringLiteral("the snapshot command answered no snapshot"));
                    if(viewports.size() != 4)
                        state->fail(QStringLiteral("the snapshot does not contain the four viewports"));
                    // The activity of the session is readable as well, and it must not contain raw inputs (D47).
                    if(snapshot.value(QStringLiteral("events")).toObject().value(QStringLiteral("events")).toArray().isEmpty())
                        state->fail(QStringLiteral("the snapshot contains no events even though the session was asked twice"));
                    if(!snapshot.value(QStringLiteral("selection")).toObject().contains(QStringLiteral("count")))
                        state->fail(QStringLiteral("the snapshot does not contain the selection"));

                    reportCheckPhase("snapshot");

                    // -------------------------------------------------------- 5. one object by ID, with its parameters
                    state->viewportId = viewports.isEmpty() ? QString() : viewports.first().toObject().value(QStringLiteral("id")).toString();
                    if(state->viewportId.isEmpty()) {
                        state->fail(QStringLiteral("the snapshot reported no viewport ID to ask about"));
                        state->continuation();
                        return;
                    }
                    runCommandLine(ui, state, { QStringLiteral("--automation"), QStringLiteral("describe"), state->viewportId, QStringLiteral("--json") }, [ui, state](bool started) {
                        if(!started) {
                            state->continuation();
                            return;
                        }
                        pollUntil(ui, 50, 30000, [state]() { return !state->process || state->process->state() == QProcess::NotRunning; }, [ui, state](bool finished) {
                            if(!finished) {
                                state->fail(QStringLiteral("the describe command did not finish"));
                                if(state->process)
                                    state->process->kill();
                                state->continuation();
                                return;
                            }
                            collectAnswer(state);
                            const QJsonObject object = state->answer.value(QStringLiteral("object")).toObject();
                            const QJsonArray properties = object.value(QStringLiteral("properties")).toArray();
                            qInfo("AUTOMATION_TEST describe %s: exit %d, kind %s, %lld parameters",
                                  qPrintable(state->viewportId), state->exitCode,
                                  qPrintable(object.value(QStringLiteral("kind")).toString()),
                                  static_cast<long long>(properties.size()));
                            if(state->exitCode != 0)
                                state->fail(QStringLiteral("the describe command reported exit code %1: %2").arg(state->exitCode).arg(state->standardError.trimmed()));
                            if(object.value(QStringLiteral("kind")).toString() != QStringLiteral("viewport"))
                                state->fail(QStringLiteral("describing a viewport ID did not answer a viewport"));
                            if(object.value(QStringLiteral("id")).toString() != state->viewportId)
                                state->fail(QStringLiteral("the described object is not the one that was asked about"));
                            // The parameters are the contract's property IDs, which is the only thing this whole stack
                            // ever handed out in a process that has the plugin classes loaded - a process without them
                            // has no property fields at all (see the note in tst_automation_contracts.cpp).
                            if(properties.isEmpty())
                                state->fail(QStringLiteral("the viewport reports no parameters"));
                            for(const QJsonValue& entry : properties) {
                                const QJsonObject property = entry.toObject();
                                const QString id = property.value(QStringLiteral("id")).toString();
                                if(!id.startsWith(QStringLiteral("property:")) || !id.contains(QStringLiteral("/")))
                                    state->fail(QStringLiteral("a parameter was reported without a property ID: %1").arg(id));
                            }

                            reportCheckPhase("describe");

                            // ---------------------------------------------------- 6. stopping the server removes the session
                            ui->stopAutomationServer();
                            QStringList problems;
                            const QVector<AutomationSessionDescriptor> remaining = AutomationLocalClient::discover(&problems);
                            qInfo("AUTOMATION_TEST sessions after stopping: %lld", static_cast<long long>(remaining.size()));
                            if(!remaining.empty())
                                state->fail(QStringLiteral("a workbench that stopped serving still publishes a session"));
                            // The session itself survives being unserved: the presentation keeps identifying objects by it.
                            if(!ui->automationSession() || ui->automationSession()->dataSet() == nullptr)
                                state->fail(QStringLiteral("stopping the server took the session away from the workbench"));
                            reportCheckPhase("stopped serving");
                            state->continuation();
                        });
                    });
                });
            });
        });
    });
}

}   // End of namespace
