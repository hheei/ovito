// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \brief The Phase 2.6 local-protocol spike (deliverable 8): one workbench, one local endpoint, one client.
 *
 * The program has three modes. `--serve` hosts an AutomationSession behind an AutomationLocalEndpoint and publishes a
 * session descriptor; `--list` shows what a client can discover right now; and the default self-test starts servers of
 * its own, connects to them as a client and checks every rule the endpoint claims to enforce - including the two
 * shutdown cases, a graceful one that removes the descriptor and a killed one that leaves a stale entry to be pruned.
 *
 * What it deliberately does *not* do is render a view. A capture needs a graphics device (a QRhi) and this tree has no
 * headless QPA plugin (the audit's O1), so an endpoint without one answers `render_unavailable` - which the self-test
 * checks as the deterministic half of the answer - while `--synthetic-capture` gives an endpoint a generated image so
 * that the transport half (bounded, encoded, in-memory artifact, no file written) is measured as well. The bridge from
 * a live viewport to an image stays Phase 5 work on the shared offscreen service; see AUTOMATION_IPC_SPIKE.md.
 *
 * Exit codes: 0 when every check passed, 1 when one failed, 2 when no session could be started at all.
 */

#include <ovito/core/app/Application.h>
#include <ovito/core/automation/AutomationContract.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>
#include <ovito/core/automation/spike/ipc/AutomationLocalClient.h>
#include <ovito/core/automation/spike/ipc/AutomationLocalEndpoint.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/viewport/ViewportConfiguration.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <iostream>
#include <memory>

using namespace Ovito;

namespace {

/******************************************************************************
* A minimal concrete Application, the one the test fixtures of the automation layer use.
*
* The spike needs an Application instance for one reason: creating the data set's scene creates scene nodes, and
* SceneNode asks Application::instance() for the main thread. It is not there to start a program - initialize() is
* never called, so no plugin is loaded and no command line is interpreted - but without it a data set cannot exist at
* all (in a release build the process would read through a null pointer).
******************************************************************************/
class IpcSpikeApplication : public Application
{
public:

    using Application::Application;

protected:

    /// Never called: initialize() is what calls it, and this program creates its own Qt application object.
    QCoreApplication* createQtApplicationImpl(bool supportGui, int& argc, char** argv) override
    {
        Q_UNUSED(supportGui);
        Q_UNUSED(argc);
        Q_UNUSED(argv);
        return nullptr;
    }
};

/******************************************************************************
* The command line of the spike.
******************************************************************************/
struct Options
{
    bool serve = false;
    bool listSessions = false;
    bool prune = false;
    bool syntheticCapture = false;
    bool allowQuit = false;
    bool verbose = false;
    QString scope;
    QStringList allowedCapabilities;
    int maximumNodes = 0;
    int maximumClients = 0;
    int pings = 20;
    QString jsonOutput;
};

/******************************************************************************
* One check of the self-test.
******************************************************************************/
struct Check
{
    QString name;
    bool ok = false;
    QString detail;

    void report(bool passed, QString information = {})
    {
        ok = passed;
        detail = std::move(information);
    }
};

/******************************************************************************
* The self-test's collected result.
******************************************************************************/
struct Report
{
    QVector<Check> checks;
    QVariantMap measurements;
    QVariantMap environment;

    Check& add(const QString& name)
    {
        checks.push_back(Check{ name, false, {} });
        return checks.back();
    }

    bool allPassed() const
    {
        for(const Check& check : checks) {
            if(!check.ok)
                return false;
        }
        return true;
    }

    int passedCount() const
    {
        int count = 0;
        for(const Check& check : checks)
            count += check.ok ? 1 : 0;
        return count;
    }
};

/// A serving child process of this program, and where it can be reached.
struct ServerHandle
{
    QProcess* process = nullptr;
    QString endpoint;
    QString sessionId;
    bool hasCaptureSource = false;
};

/// Prints one line to stdout, which is the spike's machine-readable channel.
void printLine(const QString& line)
{
    std::cout << qPrintable(line) << std::endl;
}

/******************************************************************************
* Creates the data set the served session holds: one scene node with a pipeline.
*
* An empty session would let the snapshot checks pass without proving anything, and a node with a pipeline is the shape
* a client walks - node ID, then the pipeline behind it.
******************************************************************************/
OORef<DataSet> createSpikeDataSet()
{
    OORef<DataSet> dataSet = OORef<DataSet>::create();
    ViewportConfiguration* configuration = dataSet->viewportConfig();
    if(configuration && !configuration->viewports().empty()) {
        if(Scene* scene = configuration->viewports().front()->scene()) {
            OORef<SceneNode> node = OORef<SceneNode>::create();
            node->setPipeline(OORef<Pipeline>::create());
            scene->addChildNode(node);
        }
    }
    return dataSet;
}

/******************************************************************************
* A deterministic image for the capture transport checks.
*
* It is not a rendering of anything: it exists so that the artifact rules (encoding, bounds, no file) can be measured
* in a process that has no graphics device. The report says which source answered a capture.
******************************************************************************/
QImage syntheticImage(QSize size)
{
    QImage image(size, QImage::Format_ARGB32);
    for(int y = 0; y < size.height(); ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        for(int x = 0; x < size.width(); ++x)
            row[x] = qRgb((x * 255) / qMax(1, size.width() - 1), (y * 255) / qMax(1, size.height() - 1), 128);
    }
    return image;
}

/******************************************************************************
* The server mode: hosts a session until the client says goodbye or the process is killed.
******************************************************************************/
int runServer(const Options& options, const OORef<IpcSpikeApplication>& application)
{
    // The ambient task every OVITO object of this process is created in; the Application serves as its user interface.
    std::shared_ptr<Task> task = std::make_shared<Task>();
    Task::Scope taskScope(task);
    task->setUserInterface(application);

    AutomationSession session;
    session.setDataSet(createSpikeDataSet());
    session.setUserInterface(application.get());

    AutomationLocalEndpoint endpoint(session);
    endpoint.setAllowsQuit(options.allowQuit);
    AutomationLocalEndpoint::Limits limits = endpoint.limits();
    if(options.maximumNodes > 0)
        limits.maximumSnapshotNodes = options.maximumNodes;
    if(options.maximumClients > 0)
        limits.maximumClients = options.maximumClients;
    endpoint.setLimits(limits);

    // The policy stands in for the user's consent: the read capabilities plus whatever the command line approved.
    AutomationPermissionSet policy;
    for(const QString& name : options.allowedCapabilities) {
        if(const std::optional<AutomationContract::Capability> capability = AutomationContract::capabilityFromName(name))
            policy.grant(*capability);
        else
            qWarning("Warning: '%s' is not a capability name and is not approved.", qPrintable(name));
    }
    endpoint.setGrantPolicy(policy);
    if(options.syntheticCapture)
        endpoint.setCaptureSource([](QSize size, QString*) { return syntheticImage(size); });

    QString error;
    if(!endpoint.start(&error)) {
        qWarning("Error: %s", qPrintable(error));
        return 2;
    }

    // The parent process waits for this line rather than for a descriptor file, so it cannot start talking before the
    // endpoint is listening and the session is published.
    printLine(QStringLiteral("IPC_SPIKE_SERVER endpoint=%1 session=%2 capture=%3 clients=%4")
                  .arg(endpoint.endpointName(), endpoint.descriptor().sessionId(),
                       options.syntheticCapture ? QStringLiteral("synthetic") : QStringLiteral("none"))
                  .arg(endpoint.limits().maximumClients));

    QObject::connect(&endpoint, &AutomationLocalEndpoint::quitRequested, QCoreApplication::instance(), &QCoreApplication::quit);

    // No signal handler: a SIGTERM or SIGKILL is exactly the killed case the self-test wants to observe, and the
    // graceful shutdown is the quit message.
    const int exitCode = QCoreApplication::exec();
    endpoint.stop();
    return exitCode;
}

/******************************************************************************
* The discovery mode: what a client sees right now.
******************************************************************************/
int runList(const Options& options)
{
    QStringList problems;
    const QVector<AutomationSessionDescriptor> sessions = AutomationLocalClient::discover(&problems);

    QVariantList listed;
    for(const AutomationSessionDescriptor& session : sessions) {
        listed.push_back(QVariantMap{ { QStringLiteral("sessionId"), session.sessionId() },
                                      { QStringLiteral("endpoint"), session.endpoint() },
                                      { QStringLiteral("processId"), QVariant::fromValue<qlonglong>(session.processId()) },
                                      { QStringLiteral("startedAt"), session.startedAt().toString(Qt::ISODateWithMs) },
                                      { QStringLiteral("contractVersion"), session.contractVersion() },
                                      { QStringLiteral("stale"), session.isStale() } });
    }
    QVariantMap result;
    result.insert(QStringLiteral("directory"), AutomationSessionDescriptor::directory());
    result.insert(QStringLiteral("sessions"), listed);
    result.insert(QStringLiteral("unusableFiles"), problems);
    if(options.prune) {
        QStringList removed;
        result.insert(QStringLiteral("pruned"), AutomationLocalClient::pruneStale(&removed));
        result.insert(QStringLiteral("prunedSessions"), QVariantList(removed.begin(), removed.end()));
    }
    printLine(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact)));
    return 0;
}

/******************************************************************************
* Starts a serving child process of this program and waits for its ready line.
******************************************************************************/
ServerHandle startServerChild(const Options& options, const QStringList& extraArguments)
{
    ServerHandle handle;
    handle.hasCaptureSource = extraArguments.contains(QStringLiteral("--synthetic-capture"));

    auto* process = new QProcess();
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setProgram(QCoreApplication::applicationFilePath());
    process->setArguments(QStringList{ QStringLiteral("--serve"), QStringLiteral("--scope"), options.scope } + extraArguments);
    process->start();
    if(!process->waitForStarted(15000)) {
        qWarning("Error: the server process did not start: %s", qPrintable(process->errorString()));
        delete process;
        return handle;
    }

    // The ready line names the endpoint, so the client does not have to guess which descriptor the new session is -
    // and it cannot start talking before the endpoint listens.
    QElapsedTimer timer;
    timer.start();
    QByteArray output;
    while(timer.elapsed() < 30000) {
        if(process->waitForReadyRead(1000)) {
            output += process->readAllStandardOutput();
            const QList<QByteArray> lines = output.split('\n');
            for(const QByteArray& line : lines) {
                if(!line.startsWith("IPC_SPIKE_SERVER"))
                    continue;
                for(const QByteArray& field : line.split(' ')) {
                    const int equals = field.indexOf('=');
                    if(equals < 0)
                        continue;
                    const QByteArray key = field.left(equals);
                    const QByteArray value = field.mid(equals + 1);
                    if(key == "endpoint")
                        handle.endpoint = QString::fromUtf8(value);
                    else if(key == "session")
                        handle.sessionId = QString::fromUtf8(value);
                }
                handle.process = process;
                return handle;
            }
        }
        if(process->state() == QProcess::NotRunning) {
            qWarning("Error: the server process exited with code %d: %s", int(process->exitCode()),
                     qPrintable(QString::fromUtf8(process->readAllStandardError())));
            delete process;
            return handle;
        }
    }
    qWarning("Error: the server process did not report that it is listening.");
    process->kill();
    process->waitForFinished(5000);
    delete process;
    return handle;
}

/******************************************************************************
* The client half of the self-test: every rule the endpoint claims to enforce.
******************************************************************************/
void runClientScenario(const Options& options, Report& report, const ServerHandle& server)
{
    AutomationLocalClient client;

    // ---------------------------------------------------------------- discovery
    Check& discovery = report.add(QStringLiteral("a client discovers the running session"));
    QStringList problems;
    const QVector<AutomationSessionDescriptor> sessions = AutomationLocalClient::discover(&problems);
    AutomationSessionDescriptor found;
    for(const AutomationSessionDescriptor& session : sessions) {
        if(session.endpoint() == server.endpoint)
            found = session;
    }
    discovery.report(!found.sessionId().isEmpty() && found.sessionId() == server.sessionId,
                     QStringLiteral("%1 session(s) discovered, %2 unusable file(s), the served one is %3")
                         .arg(sessions.size()).arg(problems.size()).arg(found.sessionId()));
    if(found.sessionId().isEmpty())
        return;
    Check& consistent = report.add(QStringLiteral("the descriptor of a running session is not stale"));
    consistent.report(!found.isStale() && found.processId() == qint64(server.process->processId()),
                      QStringLiteral("process %1, contract %2").arg(found.processId()).arg(found.contractVersion()));

    // ---------------------------------------------------------------- connect and handshake
    QElapsedTimer timer;
    timer.start();
    QString connectError;
    Check& connected = report.add(QStringLiteral("the client connects to the discovered endpoint"));
    connected.report(client.connectToEndpoint(server.endpoint, &connectError), connectError);
    if(!connected.ok)
        return;
    report.measurements.insert(QStringLiteral("connectMs"), double(timer.nsecsElapsed()) / 1e6);

    // A request before the handshake is refused, and that refusal is a transport error rather than a contract error.
    Check& beforeHello = report.add(QStringLiteral("a request before the handshake is answered with not_connected"));
    {
        AutomationLocalClient early;
        QString earlyError;
        if(early.connectToEndpoint(server.endpoint, &earlyError)) {
            const AutomationLocalClient::Reply earlyReply = early.dispatch(QStringLiteral("session.describe"));
            beforeHello.report(!earlyReply.transportOk && earlyReply.errorCode == QStringLiteral("not_connected"), earlyReply.errorCode);
        }
        else {
            beforeHello.report(false, earlyError);
        }
    }

    const QStringList requested{ QStringLiteral("session.read"), QStringLiteral("scene.read"),
                                 QStringLiteral("pipeline.write"), QStringLiteral("python.execute"), QStringLiteral("nonsense") };
    const AutomationLocalClient::Reply hello = client.hello(QStringLiteral("ipc spike client"), requested, QStringLiteral("cli"));
    Check& handshake = report.add(QStringLiteral("the handshake reports identity, protocol and the capability decision"));
    handshake.report(hello.transportOk && hello.result.value(QStringLiteral("clientId")).toString().startsWith(QStringLiteral("client:")),
                     QStringLiteral("clientId=%1 contract=%2 protocol=%3")
                         .arg(hello.result.value(QStringLiteral("clientId")).toString(),
                              hello.result.value(QStringLiteral("contractVersion")).toString(),
                              hello.result.value(QStringLiteral("protocol")).toString()));

    const QStringList granted = hello.result.value(QStringLiteral("grantedCapabilities")).toStringList();
    const QStringList refused = hello.result.value(QStringLiteral("refusedCapabilities")).toStringList();
    Check& capabilities = report.add(QStringLiteral("only the approved capabilities are granted, and every refusal is named"));
    capabilities.report(granted.contains(QStringLiteral("session.read")) && granted.contains(QStringLiteral("scene.read"))
                            && !granted.contains(QStringLiteral("pipeline.write")) && !granted.contains(QStringLiteral("python.execute"))
                            && refused.size() == 3,
                        QStringLiteral("granted=%1 refused=%2").arg(granted.join(u','), refused.join(u',')));
    Check& noCapabilityNoMutation = report.add(QStringLiteral("a client that was granted nothing cannot mutate anything"));
    {
        AutomationLocalClient readOnly;
        QString readOnlyError;
        if(readOnly.connectToEndpoint(server.endpoint, &readOnlyError)) {
            readOnly.hello(QStringLiteral("read-only client"), {}, QStringLiteral("ai"));
            const AutomationLocalClient::Reply denied =
                readOnly.dispatch(QStringLiteral("task.cancel"), {{ QStringLiteral("taskId"), QStringLiteral("task:t1") }});
            const QVariantMap error = denied.result.value(QStringLiteral("error")).toMap();
            const QStringList missing = error.value(QStringLiteral("details")).toMap()
                                            .value(QStringLiteral("missing")).toStringList();
            noCapabilityNoMutation.report(denied.transportOk && error.value(QStringLiteral("code")).toString() == QStringLiteral("missing_capability")
                                              && missing.contains(QStringLiteral("task.control")),
                                          QStringLiteral("%1 missing=%2").arg(error.value(QStringLiteral("code")).toString(), missing.join(u',')));
        }
        else {
            noCapabilityNoMutation.report(false, readOnlyError);
        }
    }

    // ---------------------------------------------------------------- snapshot
    timer.restart();
    const AutomationLocalClient::Reply snapshot = client.snapshot();
    report.measurements.insert(QStringLiteral("snapshotMs"), double(timer.nsecsElapsed()) / 1e6);
    const QVariantMap sessionPart = snapshot.result.value(QStringLiteral("session")).toMap();
    const QVariantMap scenePart = snapshot.result.value(QStringLiteral("scene")).toMap();
    const QVariantList nodes = scenePart.value(QStringLiteral("nodes")).toList();
    Check& snapshotShape = report.add(QStringLiteral("a snapshot describes the session, its scene and its events"));
    snapshotShape.report(snapshot.transportOk && sessionPart.value(QStringLiteral("hasDataSet")).toBool() && nodes.size() == 1
                             && scenePart.value(QStringLiteral("truncated")).toBool() == false
                             && scenePart.value(QStringLiteral("pipelines")).toMap().size() == 1
                             && snapshot.result.contains(QStringLiteral("events")) && snapshot.result.contains(QStringLiteral("tasks")),
                         QStringLiteral("nodes=%1 pipelines=%2 revision=%3")
                             .arg(nodes.size())
                             .arg(scenePart.value(QStringLiteral("pipelines")).toMap().size())
                             .arg(snapshot.result.value(QStringLiteral("revision")).toULongLong()));
    const quint64 revision = snapshot.result.value(QStringLiteral("revision")).toULongLong();

    // ---------------------------------------------------------------- dispatch
    timer.restart();
    const AutomationLocalClient::Reply described = client.dispatch(QStringLiteral("session.describe"));
    report.measurements.insert(QStringLiteral("dispatchMs"), double(timer.nsecsElapsed()) / 1e6);
    Check& dispatchOk = report.add(QStringLiteral("an operation is dispatched and its contract result is carried back"));
    dispatchOk.report(described.transportOk && described.result.value(QStringLiteral("ok")).toBool()
                          && described.result.value(QStringLiteral("revision")).toULongLong() == revision,
                      QStringLiteral("ok=%1 revision=%2")
                          .arg(described.result.value(QStringLiteral("ok")).toBool())
                          .arg(described.result.value(QStringLiteral("revision")).toULongLong()));

    Check& staleRevision = report.add(QStringLiteral("a stale baseRevision is refused deterministically"));
    {
        const AutomationLocalClient::Reply stale = client.dispatch(QStringLiteral("session.describe"), {}, revision + 10);
        const QString code = stale.result.value(QStringLiteral("error")).toMap().value(QStringLiteral("code")).toString();
        staleRevision.report(stale.transportOk && code == QStringLiteral("stale_revision"), code);
    }
    Check& unknownOperation = report.add(QStringLiteral("an unknown operation is refused with the catalog in the details"));
    {
        const AutomationLocalClient::Reply unknown = client.dispatch(QStringLiteral("scene.delete_everything"));
        const QVariantMap error = unknown.result.value(QStringLiteral("error")).toMap();
        unknownOperation.report(unknown.transportOk && error.value(QStringLiteral("code")).toString() == QStringLiteral("unknown_operation")
                                    && !error.value(QStringLiteral("details")).toMap().value(QStringLiteral("knownOperations")).toStringList().isEmpty(),
                                error.value(QStringLiteral("code")).toString());
    }
    Check& missingCapability = report.add(QStringLiteral("a capability that was not granted is refused with the names"));
    {
        const AutomationLocalClient::Reply denied =
            client.dispatch(QStringLiteral("task.cancel"), {{ QStringLiteral("taskId"), QStringLiteral("task:t1") }});
        const QVariantMap error = denied.result.value(QStringLiteral("error")).toMap();
        missingCapability.report(denied.transportOk && error.value(QStringLiteral("code")).toString() == QStringLiteral("missing_capability"),
                                 error.value(QStringLiteral("code")).toString());
    }

    // ---------------------------------------------------------------- malformed input
    Check& malformed = report.add(QStringLiteral("bytes that are not a message are answered with invalid_message"));
    {
        const AutomationLocalClient::Reply reply = client.sendRawLine("{this is not json");
        malformed.report(!reply.transportOk && reply.errorCode == QStringLiteral("invalid_message"), reply.errorCode);
    }
    Check& unsupportedMessage = report.add(QStringLiteral("a message type the endpoint does not implement is refused by name"));
    {
        const AutomationLocalClient::Reply reply = client.sendRawLine("{\"id\":9999,\"type\":\"teleport\"}");
        unsupportedMessage.report(!reply.transportOk && reply.errorCode == QStringLiteral("unsupported_message"), reply.errorCode);
    }

    // ---------------------------------------------------------------- events
    Check& subscription = report.add(QStringLiteral("a subscription hands over the retained events and then pushes new ones"));
    {
        const AutomationLocalClient::Reply subscribed = client.subscribe(0);
        const int pendingBefore = client.pushedEvents().size();
        // Dispatching anything writes an activity event into the session's log, which is what the subscription carries.
        client.dispatch(QStringLiteral("session.describe"));
        QElapsedTimer pushTimer;
        pushTimer.start();
        bool pushed = false;
        while(pushTimer.elapsed() < 5000) {
            if(client.receiveMessage(500) && client.pushedEvents().size() > pendingBefore) {
                pushed = true;
                break;
            }
        }
        const QVector<QVariantMap> events = client.pushedEvents();
        subscription.report(subscribed.transportOk && pushed && !events.isEmpty()
                                && events.back().value(QStringLiteral("kind")).toString() == QStringLiteral("activity"),
                            QStringLiteral("retained=%1 pushed=%2")
                                .arg(subscribed.result.value(QStringLiteral("events")).toList().size()).arg(pushed));
        report.measurements.insert(QStringLiteral("pushedEvents"), events.size());
        if(pushed)
            report.measurements.insert(QStringLiteral("eventPushMs"), double(pushTimer.nsecsElapsed()) / 1e6);
    }

    // ---------------------------------------------------------------- round trips
    {
        QVector<double> latencies;
        for(int i = 0; i < qMax(1, options.pings); ++i) {
            QElapsedTimer pingTimer;
            pingTimer.start();
            const AutomationLocalClient::Reply reply = client.ping();
            if(reply.transportOk)
                latencies.push_back(double(pingTimer.nsecsElapsed()) / 1e6);
        }
        if(!latencies.isEmpty()) {
            std::sort(latencies.begin(), latencies.end());
            report.measurements.insert(QStringLiteral("pingMs"), latencies[latencies.size() / 2]);
            report.measurements.insert(QStringLiteral("pingSamples"), latencies.size());
        }
        Check& pinged = report.add(QStringLiteral("the endpoint answers a round trip"));
        pinged.report(!latencies.isEmpty(), QStringLiteral("%1 round trip(s) measured").arg(latencies.size()));
    }

    // ---------------------------------------------------------------- capture
    Check& capture = report.add(server.hasCaptureSource
                                    ? QStringLiteral("a capture returns a bounded, verified image artifact and writes no file")
                                    : QStringLiteral("a capture without a graphics device is refused with render_unavailable"));
    {
        timer.restart();
        const AutomationLocalClient::Reply image = client.capture(QSize(64, 48));
        if(server.hasCaptureSource) {
            const QVariantList artifacts = image.result.value(QStringLiteral("artifacts")).toList();
            bool artifactOk = image.transportOk && artifacts.size() == 1;
            QString detail = QStringLiteral("no artifact");
            if(artifactOk) {
                const QVariantMap artifact = artifacts.front().toMap();
                const QByteArray bytes = QByteArray::fromBase64(artifact.value(QStringLiteral("base64")).toString().toLatin1());
                // The client verifies what it received rather than trusting the announced numbers, the same way the
                // transfer spike does for arrays.
                artifactOk = artifact.value(QStringLiteral("mediaType")).toString() == QStringLiteral("image/png")
                             && bytes.size() == artifact.value(QStringLiteral("bytes")).toLongLong()
                             && QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())
                                    == artifact.value(QStringLiteral("sha256")).toString()
                             && bytes.startsWith("\x89PNG")
                             && artifact.value(QStringLiteral("width")).toInt() == 64
                             && artifact.value(QStringLiteral("height")).toInt() == 48;
                detail = QStringLiteral("%1 bytes, sha256 and dimensions verified").arg(bytes.size());
                report.measurements.insert(QStringLiteral("artifactBytes"), bytes.size());
            }
            capture.report(artifactOk, detail);
        }
        else {
            capture.report(!image.transportOk && image.errorCode == QStringLiteral("render_unavailable"),
                           QStringLiteral("%1: %2").arg(image.errorCode, image.errorMessage));
        }
        report.measurements.insert(QStringLiteral("captureMs"), double(timer.nsecsElapsed()) / 1e6);
    }
    Check& oversizedCapture = report.add(QStringLiteral("a capture outside the accepted size is refused with invalid_argument"));
    {
        const AutomationLocalClient::Reply tooBig = client.capture(QSize(20000, 20000));
        oversizedCapture.report(!tooBig.transportOk && tooBig.errorCode == QStringLiteral("invalid_argument"), tooBig.errorCode);
    }

    client.disconnectFromEndpoint();
    Check& disconnected = report.add(QStringLiteral("the client can disconnect without disturbing the session"));
    {
        // The session is still there for the next client: the endpoint is a session's, not a connection's.
        AutomationLocalClient second;
        QString secondError;
        const bool reconnected = second.connectToEndpoint(server.endpoint, &secondError);
        const AutomationLocalClient::Reply greeting = reconnected ? second.hello(QStringLiteral("second client"), {}) : AutomationLocalClient::Reply{};
        disconnected.report(!client.isConnected() && reconnected && greeting.transportOk,
                            reconnected ? QStringLiteral("a second client connected after the first left") : secondError);
    }
}

/******************************************************************************
* The self-test: servers of its own, the whole client scenario, and the shutdown cases.
******************************************************************************/
int runSelfTest(const Options& options)
{
    Report report;
    report.environment.insert(QStringLiteral("scope"), options.scope);
    report.environment.insert(QStringLiteral("discoveryDirectory"), AutomationSessionDescriptor::directory());
    report.environment.insert(QStringLiteral("contractVersion"), AutomationContract::version());
    report.environment.insert(QStringLiteral("transportErrorCodes"), AutomationLocalEndpoint::transportErrorCodes());
    report.environment.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));

    // A server with an image source, so the artifact transport is measured. It approves the read capabilities and
    // task.control (for the capability check) and never pipeline.write or python.execute, which the handshake check
    // expects to be refused.
    const QStringList captureServerArguments{
        QStringLiteral("--synthetic-capture"), QStringLiteral("--allow-quit"),
        QStringLiteral("--allow"), QStringLiteral("session.read"),
        QStringLiteral("--allow"), QStringLiteral("scene.read"),
        QStringLiteral("--allow"), QStringLiteral("selection.read"),
        QStringLiteral("--allow"), QStringLiteral("file.read"),
        QStringLiteral("--allow"), QStringLiteral("task.control")
    };
    const ServerHandle server = startServerChild(options, captureServerArguments);
    Check& started = report.add(QStringLiteral("a workbench serves a local endpoint and publishes a descriptor"));
    started.report(server.process != nullptr,
                   server.process ? QStringLiteral("endpoint %1").arg(server.endpoint) : QStringLiteral("the server did not start"));
    if(!server.process)
        return 2;

    runClientScenario(options, report, server);

    // ---------------------------------------------------------------- graceful shutdown
    Check& quitRemovesDescriptor = report.add(QStringLiteral("a graceful shutdown removes the descriptor"));
    {
        AutomationLocalClient client;
        QString connectError;
        if(client.connectToEndpoint(server.endpoint, &connectError)) {
            client.hello(QStringLiteral("shutdown client"), {});
            const AutomationLocalClient::Reply stopped = client.quit();
            client.disconnectFromEndpoint();
            server.process->waitForFinished(10000);
            QStringList problems;
            const QVector<AutomationSessionDescriptor> left = AutomationLocalClient::discover(&problems);
            quitRemovesDescriptor.report(stopped.transportOk && left.isEmpty(),
                                         QStringLiteral("quit accepted=%1 sessions left=%2 exit code=%3, descriptor removed by the server")
                                             .arg(stopped.transportOk).arg(left.size()).arg(server.process->exitCode()));
        }
        else {
            quitRemovesDescriptor.report(false, connectError);
        }
    }
    delete server.process;

    // ---------------------------------------------------------------- killed shutdown
    const ServerHandle killed = startServerChild(options, { QStringLiteral("--allow"), QStringLiteral("session.read") });
    Check& killedLeavesStaleDescriptor = report.add(QStringLiteral("a killed workbench leaves a descriptor that is reported stale"));
    if(killed.process) {
        const QVector<AutomationSessionDescriptor> before = AutomationLocalClient::discover();
        bool seen = false;
        for(const AutomationSessionDescriptor& session : before)
            seen = seen || (session.endpoint() == killed.endpoint && !session.isStale());
        killed.process->kill();
        killed.process->waitForFinished(10000);
        const QStringList beforePrune = [&killed]() {
            QStringList stale;
            for(const AutomationSessionDescriptor& session : AutomationLocalClient::discover())
                if(session.endpoint() == killed.endpoint && session.isStale())
                    stale.push_back(session.sessionId());
            return stale;
        }();
        QStringList removed;
        AutomationLocalClient::pruneStale(&removed);
        const bool gone = [&killed]() {
            for(const AutomationSessionDescriptor& session : AutomationLocalClient::discover())
                if(session.endpoint() == killed.endpoint)
                    return false;
            return true;
        }();
        killedLeavesStaleDescriptor.report(seen && !beforePrune.isEmpty() && removed.contains(beforePrune.front()) && gone,
                                           QStringLiteral("seen=%1 stale=%2 pruned=%3 gone=%4")
                                               .arg(seen).arg(beforePrune.size()).arg(removed.size()).arg(gone));
        delete killed.process;
    }
    else {
        killedLeavesStaleDescriptor.report(false, QStringLiteral("the second server did not start"));
    }

    // ---------------------------------------------------------------- bounds of one reply
    Check& boundedSnapshot = report.add(QStringLiteral("a snapshot describes at most the number of nodes the endpoint allows"));
    Check& boundedClients = report.add(QStringLiteral("the endpoint refuses a client beyond its limit with too_many_clients"));
    {
        const ServerHandle small = startServerChild(options, { QStringLiteral("--maximum-nodes"), QStringLiteral("1"),
                                                              QStringLiteral("--maximum-clients"), QStringLiteral("1"),
                                                              QStringLiteral("--allow"), QStringLiteral("session.read"),
                                                              QStringLiteral("--allow"), QStringLiteral("scene.read") });
        if(small.process) {
            AutomationLocalClient client;
            QString connectError;
            if(client.connectToEndpoint(small.endpoint, &connectError)) {
                client.hello(QStringLiteral("bounded client"), { QStringLiteral("session.read"), QStringLiteral("scene.read") });
                const AutomationLocalClient::Reply snapshot = client.snapshot(1);
                const QVariantMap scene = snapshot.result.value(QStringLiteral("scene")).toMap();
                boundedSnapshot.report(snapshot.transportOk && scene.value(QStringLiteral("nodeCount")).toInt() == 1
                                           && scene.value(QStringLiteral("truncated")).toBool() == false,
                                       QStringLiteral("nodes=%1 truncated=%2 with a limit of 1")
                                           .arg(scene.value(QStringLiteral("nodeCount")).toInt())
                                           .arg(scene.value(QStringLiteral("truncated")).toBool()));

                // The endpoint serves one client at a time here, so the next connection is refused - and it is told so
                // rather than being left with a socket that goes nowhere.
                AutomationLocalClient extra;
                QString extraError;
                bool refused = false;
                QString detail;
                if(!extra.connectToEndpoint(small.endpoint, &extraError)) {
                    refused = true;
                    detail = QStringLiteral("the connection was refused: %1").arg(extraError);
                }
                else {
                    const AutomationLocalClient::Reply greeting = extra.hello(QStringLiteral("extra client"), {});
                    refused = !greeting.transportOk && greeting.errorCode == QStringLiteral("too_many_clients");
                    detail = greeting.transportOk ? QStringLiteral("the second client was greeted") : greeting.errorCode;
                }
                boundedClients.report(refused, detail);
                client.disconnectFromEndpoint();
            }
            else {
                boundedSnapshot.report(false, connectError);
                boundedClients.report(false, connectError);
            }
            small.process->kill();
            small.process->waitForFinished(10000);
            delete small.process;
        }
        else {
            boundedSnapshot.report(false, QStringLiteral("the third server did not start"));
            boundedClients.report(false, QStringLiteral("the third server did not start"));
        }
    }

    printLine(QStringLiteral("IPC_SPIKE_SUMMARY %1 checks, %2 passed, %3 failed")
                  .arg(report.checks.size()).arg(report.passedCount()).arg(report.checks.size() - report.passedCount()));
    for(const Check& check : report.checks) {
        printLine(QStringLiteral("%1 %2%3").arg(check.ok ? QStringLiteral("ok  ") : QStringLiteral("FAIL"), check.name,
                                                check.detail.isEmpty() ? QString() : QStringLiteral(" [") + check.detail + u']'));
    }
    for(auto it = report.measurements.cbegin(); it != report.measurements.cend(); ++it)
        printLine(QStringLiteral("measure %1 = %2").arg(it.key(), it.value().toString()));

    QVariantMap json;
    QVariantList checks;
    for(const Check& check : report.checks)
        checks.push_back(QVariantMap{ { QStringLiteral("name"), check.name },
                                      { QStringLiteral("passed"), check.ok },
                                      { QStringLiteral("detail"), check.detail } });
    json.insert(QStringLiteral("checks"), checks);
    json.insert(QStringLiteral("measurements"), report.measurements);
    json.insert(QStringLiteral("environment"), report.environment);
    json.insert(QStringLiteral("passed"), report.passedCount());
    json.insert(QStringLiteral("failed"), report.checks.size() - report.passedCount());
    const QByteArray reportBytes = QJsonDocument(QJsonObject::fromVariantMap(json)).toJson(QJsonDocument::Indented);
    if(!options.jsonOutput.isEmpty()) {
        QFile file(options.jsonOutput);
        if(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(reportBytes);
        else
            qWarning("Warning: cannot write '%s'.", qPrintable(options.jsonOutput));
    }
    if(options.verbose)
        printLine(QString::fromUtf8(reportBytes));

    return report.allPassed() ? 0 : 1;
}

}   // namespace

/******************************************************************************
* Program entry point.
******************************************************************************/
int main(int argc, char** argv)
{
    // The Qt application object comes first; the OVITO Application object is created on top of it, which is what the
    // test fixtures of the automation layer do as well (QTest owns the QCoreApplication there).
    QCoreApplication qtApplication(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ovito-automation-ipc-spike"));
    QCoreApplication::setOrganizationName(QStringLiteral("OVITO"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Phase 2.6 local-protocol spike: one local endpoint, one client."));
    parser.addHelpOption();
    const QCommandLineOption serveOption({ QStringLiteral("serve") }, QStringLiteral("Serve a session and publish a descriptor."));
    const QCommandLineOption listOption({ QStringLiteral("list") }, QStringLiteral("List the sessions a client can discover."));
    const QCommandLineOption pruneOption({ QStringLiteral("prune") }, QStringLiteral("With --list: remove the descriptors whose process is gone."));
    const QCommandLineOption scopeOption(QStringLiteral("scope"), QStringLiteral("The discovery directory to use."), QStringLiteral("DIR"));
    const QCommandLineOption allowOption(QStringLiteral("allow"), QStringLiteral("Approve a capability for connecting clients."), QStringLiteral("NAME"));
    const QCommandLineOption maximumNodesOption(QStringLiteral("maximum-nodes"), QStringLiteral("The number of scene nodes a snapshot describes."), QStringLiteral("N"));
    const QCommandLineOption maximumClientsOption(QStringLiteral("maximum-clients"), QStringLiteral("The number of clients the endpoint serves at once."), QStringLiteral("N"));
    const QCommandLineOption captureOption(QStringLiteral("synthetic-capture"), QStringLiteral("Answer captures with a generated image."));
    const QCommandLineOption allowQuitOption(QStringLiteral("allow-quit"), QStringLiteral("Accept the quit message of a client."));
    const QCommandLineOption pingsOption(QStringLiteral("pings"), QStringLiteral("Ping round trips to measure."), QStringLiteral("N"));
    const QCommandLineOption jsonOption(QStringLiteral("json"), QStringLiteral("Write the report to a file."), QStringLiteral("FILE"));
    const QCommandLineOption verboseOption({ QStringLiteral("verbose") }, QStringLiteral("Print the report as JSON as well."));
    parser.addOptions({ serveOption, listOption, pruneOption, scopeOption, allowOption, maximumNodesOption, maximumClientsOption,
                        captureOption, allowQuitOption, pingsOption, jsonOption, verboseOption });
    parser.process(qtApplication);

    Options options;
    options.serve = parser.isSet(serveOption);
    options.listSessions = parser.isSet(listOption);
    options.prune = parser.isSet(pruneOption);
    options.syntheticCapture = parser.isSet(captureOption);
    options.allowQuit = parser.isSet(allowQuitOption);
    options.verbose = parser.isSet(verboseOption);
    options.scope = parser.value(scopeOption);
    options.allowedCapabilities = parser.values(allowOption);
    options.maximumNodes = parser.isSet(maximumNodesOption) ? parser.value(maximumNodesOption).toInt() : 0;
    options.maximumClients = parser.isSet(maximumClientsOption) ? parser.value(maximumClientsOption).toInt() : 0;
    options.pings = parser.isSet(pingsOption) ? parser.value(pingsOption).toInt() : 20;
    options.jsonOutput = parser.value(jsonOption);

    // A private discovery scope keeps a spike run from colliding with whatever else the user is running - and from
    // publishing a session that another client would try to connect to.
    if(options.scope.isEmpty())
        options.scope = QDir::tempPath() + QStringLiteral("/ovito-automation-ipc-spike-%1").arg(QCoreApplication::applicationPid());
    qputenv("OVITO_AUTOMATION_SESSION_DIR", options.scope.toLocal8Bit());

    // The Application object exists for the object model, not for a program: it is what makes creating a scene node
    // possible at all, and it is retired the way the test fixtures retire theirs.
    OORef<IpcSpikeApplication> application = OORef<IpcSpikeApplication>::create();

    int exitCode = 0;
    if(options.serve)
        exitCode = runServer(options, application);
    else if(options.listSessions)
        exitCode = runList(options);
    else
        exitCode = runSelfTest(options);

    application->taskManager().requestShutdown();
    return exitCode;
}
