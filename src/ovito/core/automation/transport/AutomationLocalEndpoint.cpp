// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "AutomationLocalEndpoint.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

#include <algorithm>

namespace Ovito {

namespace {

/// The transport-level error codes. They are deliberately disjoint from the contract's vocabulary: a client that reads
/// `invalid_message` knows the endpoint did not understand the bytes, while `invalid_argument` in an embedded result
/// means the workbench understood and refused the operation.
const QStringList& transportCodes()
{
    static const QStringList codes = {
        QStringLiteral("invalid_message"),      // the line is not a JSON object, or has no usable type
        QStringLiteral("too_large"),            // the line or the artifact exceeds what the endpoint accepts
        QStringLiteral("not_connected"),        // a message arrived before the hello message
        QStringLiteral("unsupported_message"),  // a message type this endpoint does not implement
        QStringLiteral("too_many_clients"),     // the endpoint is full
        QStringLiteral("render_unavailable"),   // capture is not possible in this process
        QStringLiteral("internal_error")        // the endpoint itself failed
    };
    return codes;
}

/// Encodes an image as PNG bytes, or returns an empty array and a reason.
QByteArray encodePng(const QImage& image, QString* error)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if(!buffer.open(QIODevice::WriteOnly)) {
        if(error)
            *error = QStringLiteral("cannot open an in-memory buffer");
        return {};
    }
    if(!image.save(&buffer, "PNG")) {
        if(error)
            *error = QStringLiteral("Qt could not encode the image as PNG");
        return {};
    }
    buffer.close();
    return bytes;
}

}   // namespace

/******************************************************************************
* Constructor.
******************************************************************************/
AutomationLocalEndpoint::AutomationLocalEndpoint(AutomationSession& session, QObject* parent)
    : QObject(parent), _session(session)
{
    connect(&_server, &QLocalServer::newConnection, this, &AutomationLocalEndpoint::onNewConnection);
}

/******************************************************************************
* Destructor.
******************************************************************************/
AutomationLocalEndpoint::~AutomationLocalEndpoint()
{
    stop();
}

/******************************************************************************
* The transport-level error codes.
******************************************************************************/
QStringList AutomationLocalEndpoint::transportErrorCodes()
{
    return transportCodes();
}

/******************************************************************************
* Sets the most a client can be granted.
******************************************************************************/
void AutomationLocalEndpoint::setGrantPolicy(const AutomationPermissionSet& policy)
{
    _policy = policy;
}

/******************************************************************************
* Starts listening and publishes the session descriptor.
******************************************************************************/
bool AutomationLocalEndpoint::start(QString* error)
{
    if(_server.isListening()) {
        if(error)
            *error = QStringLiteral("the endpoint is already listening");
        return false;
    }

    _descriptor = AutomationSessionDescriptor::allocateNewSession(QStringLiteral("ovito-automation"));
    const QString name = _descriptor.endpoint();

    // The endpoint is a socket path inside the session directory, so that the socket - not only the descriptor - is
    // reachable by its owner alone; the directory has to exist before the socket can be created in it, and it is
    // created with owner-only permissions for the same reason.
    QDir scopeDirectory(AutomationSessionDescriptor::directory());
    if(!scopeDirectory.exists() && !scopeDirectory.mkpath(QStringLiteral("."))) {
        if(error)
            *error = QStringLiteral("cannot create the session directory '%1'").arg(scopeDirectory.absolutePath());
        return false;
    }
#if defined(Q_OS_UNIX)
    QFile::setPermissions(scopeDirectory.absolutePath(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif

    // A leftover socket file of a crash keeps QLocalServer::listen() from succeeding, and only the descriptor tells
    // whether the session that created it is still there. Removing a socket whose process is gone is exactly the
    // cleanup the design asks for, and removing one whose process is alive fails harmlessly (the listen() below then
    // answers with an error instead).
    const QVector<AutomationSessionDescriptor> known = AutomationSessionDescriptor::discover();
    for(const AutomationSessionDescriptor& found : known) {
        if(found.endpoint() == name && found.isStale())
            QLocalServer::removeServer(name);
    }

    // Only the user who owns the session may connect to its socket, which is the second half of the protection the
    // descriptor's file permissions provide.
#if defined(Q_OS_UNIX)
    _server.setSocketOptions(QLocalServer::UserAccessOption);
#endif

    if(!_server.listen(name)) {
        if(error)
            *error = QStringLiteral("cannot listen on '%1': %2").arg(name, _server.errorString());
        return false;
    }

    QString writeError;
    if(!_descriptor.writeToDisk(&writeError)) {
        _server.close();
        if(error)
            *error = QStringLiteral("cannot publish the session descriptor: %1").arg(writeError);
        return false;
    }

    // Push events to the clients that subscribed. The log is bounded, so a client that falls behind is told where it
    // can resume from rather than being handed a stream that silently misses entries.
    _eventConnection = connect(&_session.events(), &AutomationEventLog::eventAppended, this,
                              &AutomationLocalEndpoint::onEventAppended);
    return true;
}

/******************************************************************************
* Closes the endpoint and removes the descriptor.
******************************************************************************/
void AutomationLocalEndpoint::stop()
{
    disconnect(_eventConnection);
    _eventConnection = {};

    while(!_clients.empty())
        closeClient(*_clients.back(), QStringLiteral("the endpoint is closing"));

    if(_server.isListening())
        _server.close();
    // Never leave a descriptor behind for a session that cannot be reached: a client that finds it would try to
    // connect to a socket that no longer exists.
    if(!_descriptor.sessionId().isEmpty())
        _descriptor.removeFromDisk();
}

/******************************************************************************
* Accepts a connection if the endpoint has room for it.
******************************************************************************/
void AutomationLocalEndpoint::onNewConnection()
{
    while(QLocalSocket* socket = _server.nextPendingConnection()) {
        if(static_cast<int>(_clients.size()) >= _limits.maximumClients) {
            // Refuse by answering with the transport error and then closing, so a client learns why instead of seeing
            // a connection that goes nowhere.
            _statistics.rejectedConnections++;
            // The refusal uses the same envelope as every other reply - including the type that makes it recognizable as
            // an answer rather than a pushed event - because a client that is turned away still has to be able to read it.
            QVariantMap reply = makeReply(QVariant());
            reply.insert(QStringLiteral("ok"), false);
            QVariantMap error;
            error.insert(QStringLiteral("code"), QStringLiteral("too_many_clients"));
            error.insert(QStringLiteral("message"), QStringLiteral("this endpoint serves at most %1 clients").arg(_limits.maximumClients));
            reply.insert(QStringLiteral("error"), error);
            socket->write(QJsonDocument(QJsonObject::fromVariantMap(reply)).toJson(QJsonDocument::Compact) + '\n');
            socket->flush();
            socket->disconnectFromServer();
            socket->deleteLater();
            continue;
        }

        auto client = std::make_unique<Client>();
        client->socket = socket;
        // A local socket names its peer only on some platforms; what identifies the client is its handshake.
        connect(socket, &QLocalSocket::readyRead, this, &AutomationLocalEndpoint::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &AutomationLocalEndpoint::onDisconnected);
        _clients.push_back(std::move(client));
        _statistics.connections++;
    }
}

/******************************************************************************
* Finds the client of a socket.
******************************************************************************/
AutomationLocalEndpoint::Client* AutomationLocalEndpoint::clientFor(const QLocalSocket* socket) const
{
    for(const std::unique_ptr<Client>& client : _clients) {
        if(client->socket == socket)
            return client.get();
    }
    return nullptr;
}

/******************************************************************************
* Reads complete lines from a socket.
******************************************************************************/
void AutomationLocalEndpoint::onReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    Client* client = socket ? clientFor(socket) : nullptr;
    if(!client)
        return;

    client->buffer += socket->readAll();
    while(true) {
        const int newline = client->buffer.indexOf('\n');
        if(newline < 0) {
            // A line that never ends is not a large message, it is a client that is not speaking this protocol.
            if(client->buffer.size() > _limits.maximumRequestBytes) {
                _statistics.transportErrors++;
                sendError(*client, QVariant(), QStringLiteral("too_large"),
                          QStringLiteral("the request is longer than %1 bytes").arg(_limits.maximumRequestBytes));
                closeClient(*client, QStringLiteral("oversized request"));
                return;
            }
            return;
        }
        if(newline > _limits.maximumRequestBytes) {
            _statistics.transportErrors++;
            sendError(*client, QVariant(), QStringLiteral("too_large"),
                      QStringLiteral("the request is longer than %1 bytes").arg(_limits.maximumRequestBytes));
            closeClient(*client, QStringLiteral("oversized request"));
            return;
        }
        const QByteArray line = client->buffer.left(newline);
        client->buffer.remove(0, newline + 1);
        if(line.trimmed().isEmpty())
            continue;

        // serveLine() may close and drop the client (a bye message, an unsupported handshake), so the socket is the
        // only thing this loop may look at afterwards.
        serveLine(*client, line);
        if(clientFor(socket) == nullptr)
            return;
    }
}

/******************************************************************************
* Handles a client that went away.
******************************************************************************/
void AutomationLocalEndpoint::onDisconnected()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    Client* client = socket ? clientFor(socket) : nullptr;
    if(!client)
        return;
    closeClient(*client, QStringLiteral("the client disconnected"));
}

/******************************************************************************
* Answers one request line.
******************************************************************************/
void AutomationLocalEndpoint::serveLine(Client& client, const QByteArray& line)
{
    _statistics.requests++;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if(parseError.error != QJsonParseError::NoError || !document.isObject()) {
        _statistics.transportErrors++;
        sendError(client, QVariant(), QStringLiteral("invalid_message"),
                  QStringLiteral("the request is not a JSON object: %1").arg(parseError.errorString()));
        return;
    }

    const QVariantMap message = document.object().toVariantMap();
    const QVariant requestId = message.value(QStringLiteral("id"));
    const QString type = message.value(QStringLiteral("type")).toString();
    if(type.isEmpty()) {
        _statistics.transportErrors++;
        sendError(client, requestId, QStringLiteral("invalid_message"), QStringLiteral("the request names no type"));
        return;
    }

    const auto replyTo = [this, &requestId]() { return makeReply(requestId); };

    if(type == QStringLiteral("hello")) {
        QVariantMap reply = replyTo();
        handleHello(client, message, reply);
        sendReply(client, reply);
        return;
    }
    if(!client.greeted) {
        // Only hello and ping are available before the handshake: anything else would be answered with a capability
        // decision that was never made.
        if(type == QStringLiteral("ping")) {
            QVariantMap reply = replyTo();
            reply.insert(QStringLiteral("ok"), true);
            QVariantMap result;
            result.insert(QStringLiteral("pong"), true);
            result.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_session.revision()));
            reply.insert(QStringLiteral("result"), result);
            sendReply(client, reply);
            return;
        }
        _statistics.transportErrors++;
        sendError(client, requestId, QStringLiteral("not_connected"),
                  QStringLiteral("send a hello message before '%1'").arg(type));
        return;
    }

    if(type == QStringLiteral("ping")) {
        QVariantMap reply = replyTo();
        reply.insert(QStringLiteral("ok"), true);
        QVariantMap result;
        result.insert(QStringLiteral("pong"), true);
        result.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_session.revision()));
        reply.insert(QStringLiteral("result"), result);
        sendReply(client, reply);
        return;
    }
    if(type == QStringLiteral("dispatch")) {
        QVariantMap reply = replyTo();
        handleDispatch(client, message, reply);
        sendReply(client, reply);
        return;
    }
    if(type == QStringLiteral("snapshot")) {
        QVariantMap reply = replyTo();
        handleSnapshot(client, message, reply);
        sendReply(client, reply);
        return;
    }
    if(type == QStringLiteral("subscribe") || type == QStringLiteral("unsubscribe")) {
        QVariantMap reply = replyTo();
        handleSubscribe(client, message, reply);
        sendReply(client, reply);
        return;
    }
    if(type == QStringLiteral("capture")) {
        QVariantMap reply = replyTo();
        handleCapture(client, message, reply);
        sendReply(client, reply);
        return;
    }
    if(type == QStringLiteral("quit")) {
        if(!_allowsQuit) {
            _statistics.transportErrors++;
            sendError(client, requestId, QStringLiteral("unsupported_message"),
                      QStringLiteral("this endpoint does not accept a quit message"));
            return;
        }
        QVariantMap reply = replyTo();
        reply.insert(QStringLiteral("ok"), true);
        QVariantMap result;
        result.insert(QStringLiteral("stopping"), true);
        reply.insert(QStringLiteral("result"), result);
        sendReply(client, reply);
        Q_EMIT quitRequested();
        return;
    }
    if(type == QStringLiteral("bye")) {
        QVariantMap reply = replyTo();
        reply.insert(QStringLiteral("ok"), true);
        sendReply(client, reply);
        closeClient(client, QStringLiteral("the client said goodbye"));
        return;
    }

    _statistics.transportErrors++;
    sendError(client, requestId, QStringLiteral("unsupported_message"),
              QStringLiteral("'%1' is not a message type of this endpoint, which speaks %2")
                  .arg(type, QStringList({ QStringLiteral("hello"), QStringLiteral("ping"), QStringLiteral("dispatch"),
                                           QStringLiteral("snapshot"), QStringLiteral("subscribe"),
                                           QStringLiteral("unsubscribe"), QStringLiteral("capture"),
                                           QStringLiteral("bye") }).join(QStringLiteral(", "))));
}

/******************************************************************************
* The handshake: identity, origin and the capabilities the client asks for.
******************************************************************************/
void AutomationLocalEndpoint::handleHello(Client& client, const QVariantMap& message, QVariantMap& reply)
{
    if(client.greeted) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("invalid_message"));
        error.insert(QStringLiteral("message"), QStringLiteral("this connection sent a hello message already"));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    // The protocol version the client speaks has to be the one this endpoint answers with. A minor difference is
    // compatible by the contract's own rule, a major difference is not.
    const QString protocol = message.value(QStringLiteral("protocol"), AutomationContract::version()).toString();
    if(protocol.section(u'.', 0, 0) != AutomationContract::version().section(u'.', 0, 0)) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("unsupported_message"));
        error.insert(QStringLiteral("message"), QStringLiteral("this endpoint speaks contract version %1, the client speaks %2")
                                                    .arg(AutomationContract::version(), protocol));
        error.insert(QStringLiteral("details"), QVariantMap{{ QStringLiteral("contractVersion"), AutomationContract::version() }});
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    const QString name = message.value(QStringLiteral("client"), QStringLiteral("local client")).toString();
    const std::optional<AutomationContract::ActivityOrigin> origin =
        AutomationContract::originFromName(message.value(QStringLiteral("origin"), QStringLiteral("cli")).toString());
    if(!origin) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("invalid_message"));
        error.insert(QStringLiteral("message"), QStringLiteral("'%1' is not an activity origin")
                                                    .arg(message.value(QStringLiteral("origin")).toString()));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    // One gateway per connection: the identity, the capabilities and the recorded activity belong to this client
    // alone, which is what makes "who did that" answerable in the session's log.
    client.name = name;
    client.gateway = new AutomationGateway(_session, *origin, name, this);
    client.id = client.gateway->clientId();

    // Grant what the policy allows of what the client asked for, and say what was refused. The read-only defaults the
    // gateway starts with are replaced by this decision, so a client never holds more than the policy allows.
    QStringList requested;
    for(const QVariant& entry : message.value(QStringLiteral("capabilities")).toList())
        requested.push_back(entry.toString());
    QStringList granted;
    QStringList refused;
    client.gateway->clearCapabilities();
    for(const QString& requestedName : requested) {
        const std::optional<AutomationContract::Capability> capability = AutomationContract::capabilityFromName(requestedName);
        if(!capability) {
            refused.push_back(requestedName + QStringLiteral(" (no such capability)"));
            continue;
        }
        if(!_policy.contains(*capability)) {
            refused.push_back(requestedName + QStringLiteral(" (not approved for this session)"));
            continue;
        }
        // grantCapability() records the grant as activity, so the session's log tells who handed out what.
        client.gateway->grantCapability(*capability);
        granted.push_back(requestedName);
    }
    client.greeted = true;

    QVariantMap result;
    result.insert(QStringLiteral("clientId"), client.id);
    result.insert(QStringLiteral("clientName"), name);
    result.insert(QStringLiteral("origin"), AutomationContract::originName(*origin));
    result.insert(QStringLiteral("contractVersion"), AutomationContract::version());
    result.insert(QStringLiteral("protocol"), QStringLiteral("jsonl/1"));
    result.insert(QStringLiteral("grantedCapabilities"), granted);
    result.insert(QStringLiteral("refusedCapabilities"), refused);
    result.insert(QStringLiteral("hasDataSet"), _session.dataSet() != nullptr);
    result.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_session.revision()));
    result.insert(QStringLiteral("transportErrors"), transportCodes());
    reply.insert(QStringLiteral("ok"), true);
    reply.insert(QStringLiteral("result"), result);

    Q_EMIT clientConnected(client.id, client.name);
}

/******************************************************************************
* Dispatches one operation through the client's own gateway.
******************************************************************************/
void AutomationLocalEndpoint::handleDispatch(Client& client, const QVariantMap& message, QVariantMap& reply)
{
    const QString operation = message.value(QStringLiteral("operation")).toString();
    if(operation.isEmpty()) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("invalid_message"));
        error.insert(QStringLiteral("message"), QStringLiteral("a dispatch message names no operation"));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    AutomationRequest request(operation);
    request.setArguments(message.value(QStringLiteral("arguments")).toMap());
    if(message.contains(QStringLiteral("baseRevision")))
        request.setBaseRevision(message.value(QStringLiteral("baseRevision")).toULongLong());
    if(message.contains(QStringLiteral("requestId")))
        request.setRequestId(message.value(QStringLiteral("requestId")).toString());

    const AutomationResult result = client.gateway->dispatch(request);
    if(result.isError())
        _statistics.contractErrors++;

    // The transport understood the message (ok: true) and carries the contract's own answer, whose ok may be false.
    // Hoisting the contract's failure into the envelope would lose the distinction between "not understood" and
    // "understood and refused", which is the one a client has to make first.
    reply.insert(QStringLiteral("ok"), true);
    reply.insert(QStringLiteral("result"), result.toJson());
}

/******************************************************************************
* Answers a bounded snapshot of the session.
******************************************************************************/
void AutomationLocalEndpoint::handleSnapshot(Client& client, const QVariantMap& message, QVariantMap& reply)
{
    // The snapshot is a composition of contract answers, not a second implementation of what a session contains: the
    // client sees exactly what session.describe, scene.list_nodes and pipeline.describe say, with a bound on how much
    // of it one reply carries.
    const int maximumNodes = message.contains(QStringLiteral("maxNodes"))
                                 ? std::clamp(message.value(QStringLiteral("maxNodes")).toInt(), 1, 10000)
                                 : _limits.maximumSnapshotNodes;

    const AutomationResult sessionDescription = client.gateway->dispatch(AutomationRequest(QStringLiteral("session.describe")));
    if(sessionDescription.isError()) {
        _statistics.contractErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("internal_error"));
        error.insert(QStringLiteral("message"), QStringLiteral("the session cannot be described: %1").arg(sessionDescription.errorMessage()));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    const AutomationResult nodeList = client.gateway->dispatch(AutomationRequest(QStringLiteral("scene.list_nodes")));
    const QVariantList allNodes = nodeList.isSuccess() ? nodeList.data().value(QStringLiteral("nodes")).toList() : QVariantList();
    QVariantList nodes;
    QVariantMap pipelines;
    for(const QVariant& entry : allNodes) {
        if(nodes.size() >= maximumNodes)
            break;
        const QVariantMap node = entry.toMap();
        nodes.push_back(node);
        const QString pipelineId = node.value(QStringLiteral("pipelineId")).toString();
        if(pipelineId.isEmpty() || pipelines.contains(pipelineId))
            continue;
        const AutomationResult pipeline = client.gateway->dispatch(
            AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{ QStringLiteral("pipelineId"), pipelineId }}));
        // A pipeline that cannot be described is reported as such inside the node rather than failing the snapshot:
        // a client inspecting a session should still learn what is in it.
        pipelines.insert(pipelineId, pipeline.isSuccess()
                                        ? pipeline.data()
                                        : QVariantMap{{ QStringLiteral("error"), pipeline.errorMessage() }});
    }

    QVariantMap scene;
    scene.insert(QStringLiteral("nodes"), nodes);
    scene.insert(QStringLiteral("nodeCount"), allNodes.size());
    scene.insert(QStringLiteral("truncated"), allNodes.size() > nodes.size());
    scene.insert(QStringLiteral("pipelines"), pipelines);

    reply.insert(QStringLiteral("ok"), true);
    QVariantMap result;
    result.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_session.revision()));
    result.insert(QStringLiteral("session"), sessionDescription.data());
    result.insert(QStringLiteral("scene"), scene);
    // The viewports and the selection are part of what the session looks like, and they are two more contract answers
    // rather than a second description of them.
    result.insert(QStringLiteral("viewports"), client.gateway->dispatch(AutomationRequest(QStringLiteral("viewport.list"))).data());
    result.insert(QStringLiteral("selection"), client.gateway->dispatch(AutomationRequest(QStringLiteral("selection.describe"))).data());
    result.insert(QStringLiteral("tasks"), client.gateway->dispatch(AutomationRequest(QStringLiteral("task.list"))).data());
    result.insert(QStringLiteral("events"), client.gateway->dispatch(
                      AutomationRequest(QStringLiteral("event.list"))
                          .setArguments({{ QStringLiteral("limit"), _limits.maximumEvents }})).data());
    reply.insert(QStringLiteral("result"), result);
}

/******************************************************************************
* Subscribes a client to the session's events, or ends the subscription.
******************************************************************************/
void AutomationLocalEndpoint::handleSubscribe(Client& client, const QVariantMap& message, QVariantMap& reply)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if(type == QStringLiteral("unsubscribe")) {
        client.subscribed = false;
        reply.insert(QStringLiteral("ok"), true);
        reply.insert(QStringLiteral("result"), QVariantMap{{ QStringLiteral("subscribed"), false }});
        return;
    }

    // A subscription starts by handing over what the client missed, so a client that reconnects does not have to poll
    // to catch up and the log's bound is visible to it rather than something it has to infer.
    const quint64 since = message.value(QStringLiteral("since")).toULongLong();
    QVariantList events;
    for(const AutomationEvent& event : _session.events().events(since, _limits.maximumEvents))
        events.push_back(event.toJson());
    client.subscribed = true;
    client.lastEventSequence = _session.events().lastSequence();

    QVariantMap result;
    result.insert(QStringLiteral("subscribed"), true);
    result.insert(QStringLiteral("firstRetainedSequence"), QVariant::fromValue<qulonglong>(_session.events().firstSequence()));
    result.insert(QStringLiteral("lastSequence"), QVariant::fromValue<qulonglong>(_session.events().lastSequence()));
    // A client that asked from a sequence the log no longer reaches is told that its missed events are gone, which is
    // the difference between a gap and an empty history.
    result.insert(QStringLiteral("missedEvents"), since < _session.events().firstSequence() && since != 0);
    result.insert(QStringLiteral("events"), events);
    reply.insert(QStringLiteral("ok"), true);
    reply.insert(QStringLiteral("result"), result);
}

/******************************************************************************
* Answers a capture request with a bounded, in-memory image artifact.
******************************************************************************/
void AutomationLocalEndpoint::handleCapture(Client& client, const QVariantMap& message, QVariantMap& reply)
{
    const int width = message.value(QStringLiteral("width"), 320).toInt();
    const int height = message.value(QStringLiteral("height"), 240).toInt();
    if(width <= 0 || height <= 0 || width > _limits.maximumCaptureSize.width() || height > _limits.maximumCaptureSize.height()) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("invalid_argument"));
        error.insert(QStringLiteral("message"), QStringLiteral("a capture of %1x%2 is outside what this endpoint returns (at most %3x%4)")
                                                    .arg(width).arg(height)
                                                    .arg(_limits.maximumCaptureSize.width()).arg(_limits.maximumCaptureSize.height()));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    if(!_captureSource) {
        // The honest answer of a process that has no graphics device. Rendering a view needs a QRhi; a core-only
        // process has none (there is no headless QPA plugin in this tree, see the audit's O1), so the endpoint says so
        // instead of returning an empty image a client would have to interpret.
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("render_unavailable"));
        error.insert(QStringLiteral("message"), QStringLiteral("this endpoint has no image source: it was started without a graphics device"));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    QString sourceError;
    const QImage image = _captureSource(QSize(width, height), &sourceError);
    if(image.isNull()) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("render_unavailable"));
        error.insert(QStringLiteral("message"), sourceError.isEmpty() ? QStringLiteral("the image source returned nothing") : sourceError);
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    QString encodeError;
    const QByteArray png = encodePng(image, &encodeError);
    if(png.isEmpty()) {
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("internal_error"));
        error.insert(QStringLiteral("message"), encodeError);
        reply.insert(QStringLiteral("error"), error);
        return;
    }
    if(png.size() > _limits.maximumArtifactBytes) {
        // The bound is on the artifact, not only on the request: a client cannot make the endpoint produce a reply it
        // promised not to produce.
        _statistics.transportErrors++;
        reply.insert(QStringLiteral("ok"), false);
        QVariantMap error;
        error.insert(QStringLiteral("code"), QStringLiteral("too_large"));
        error.insert(QStringLiteral("message"), QStringLiteral("the encoded image has %1 bytes, more than the %2 this endpoint returns")
                                                    .arg(png.size()).arg(_limits.maximumArtifactBytes));
        reply.insert(QStringLiteral("error"), error);
        return;
    }

    // In memory, and never on disk: the endpoint writes no file, so a capture cannot be smuggled into a path.
    QVariantMap artifact;
    artifact.insert(QStringLiteral("mediaType"), QStringLiteral("image/png"));
    artifact.insert(QStringLiteral("width"), image.width());
    artifact.insert(QStringLiteral("height"), image.height());
    artifact.insert(QStringLiteral("bytes"), QVariant::fromValue<qlonglong>(png.size()));
    artifact.insert(QStringLiteral("sha256"), QString::fromLatin1(QCryptographicHash::hash(png, QCryptographicHash::Sha256).toHex()));
    artifact.insert(QStringLiteral("base64"), QString::fromLatin1(png.toBase64()));
    _statistics.artifacts++;

    reply.insert(QStringLiteral("ok"), true);
    QVariantMap result;
    result.insert(QStringLiteral("artifacts"), QVariantList{ artifact });
    reply.insert(QStringLiteral("result"), result);
}

/******************************************************************************
* Builds the envelope of a reply.
******************************************************************************/
QVariantMap AutomationLocalEndpoint::makeReply(const QVariant& requestId) const
{
    QVariantMap reply;
    if(requestId.isValid())
        reply.insert(QStringLiteral("id"), requestId);
    reply.insert(QStringLiteral("type"), QStringLiteral("reply"));
    reply.insert(QStringLiteral("contractVersion"), AutomationContract::version());
    reply.insert(QStringLiteral("transport"), QStringLiteral("jsonl/1"));
    return reply;
}

/******************************************************************************
* Sends a reply.
******************************************************************************/
void AutomationLocalEndpoint::sendReply(Client& client, const QVariantMap& reply)
{
    if(!client.socket || client.socket->state() != QLocalSocket::ConnectedState)
        return;
    const bool transportOk = reply.value(QStringLiteral("ok")).toBool();
    Q_EMIT requestHandled(reply.value(QStringLiteral("type")).toString(), transportOk);
    client.socket->write(QJsonDocument(QJsonObject::fromVariantMap(reply)).toJson(QJsonDocument::Compact) + '\n');
    client.socket->flush();
}

/******************************************************************************
* Answers a request with a transport-level error.
******************************************************************************/
void AutomationLocalEndpoint::sendError(Client& client, const QVariant& requestId, const QString& code, const QString& message,
                                        QVariantMap details)
{
    QVariantMap reply = makeReply(requestId);
    reply.insert(QStringLiteral("ok"), false);
    QVariantMap error;
    error.insert(QStringLiteral("code"), code);
    error.insert(QStringLiteral("message"), message);
    if(!details.isEmpty())
        error.insert(QStringLiteral("details"), details);
    reply.insert(QStringLiteral("error"), error);
    sendReply(client, reply);
}

/******************************************************************************
* Pushes one event to the subscribed clients.
******************************************************************************/
void AutomationLocalEndpoint::onEventAppended(quint64 sequence)
{
    if(_clients.empty())
        return;
    const QVector<AutomationEvent> retained = _session.events().events(sequence - 1, 1);
    if(retained.isEmpty())
        return;
    QVariantMap message;
    message.insert(QStringLiteral("type"), QStringLiteral("event"));
    message.insert(QStringLiteral("event"), retained.front().toJson());

    for(const std::unique_ptr<Client>& client : _clients) {
        if(!client->subscribed || !client->socket || client->socket->state() != QLocalSocket::ConnectedState)
            continue;
        // An event that arrives faster than a client reads fills the socket buffer, which is a property of a client
        // that does not keep up, not a reason to drop its subscription.
        client->lastEventSequence = sequence;
        client->socket->write(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact) + '\n');
        client->socket->flush();
        _statistics.eventsPushed++;
    }
}

/******************************************************************************
* Ends one connection and forgets its client.
******************************************************************************/
void AutomationLocalEndpoint::closeClient(Client& client, const QString& reason)
{
    Q_UNUSED(reason);

    // Take the client out of the list *before* its socket is touched. Disconnecting a socket can emit `disconnected`
    // synchronously - a socket that is already in the closing state does - which would re-enter this function through
    // onDisconnected() and destroy the object this frame is still working on. Owning the client locally and removing
    // it first makes the re-entrant call find nothing to do, and keeps the object alive until this frame is done.
    std::unique_ptr<Client> owned;
    for(auto it = _clients.begin(); it != _clients.end(); ++it) {
        if(it->get() == &client) {
            owned = std::move(*it);
            _clients.erase(it);
            break;
        }
    }
    if(!owned)
        return;   // an outer frame is closing this client already

    const QString clientId = client.id;

    if(client.socket) {
        if(client.socket->state() == QLocalSocket::ConnectedState) {
            client.socket->flush();
            client.socket->disconnectFromServer();
        }
        // No signal of this socket may arrive while it is being torn down, and none may arrive after it is gone.
        client.socket->disconnect(this);
        client.socket->deleteLater();
        client.socket = nullptr;
    }
    if(client.gateway) {
        client.gateway->disconnect(this);
        client.gateway->deleteLater();
        client.gateway = nullptr;
    }

    if(!clientId.isEmpty())
        Q_EMIT clientDisconnected(clientId);
}

}   // namespace Ovito
