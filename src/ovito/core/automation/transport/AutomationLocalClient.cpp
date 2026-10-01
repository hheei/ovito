// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "AutomationLocalClient.h"

#include <ovito/core/automation/AutomationContract.h>

#include <QJsonDocument>
#include <QElapsedTimer>

namespace Ovito {

namespace {

/// The transport-level codes the client is written against. They are listed rather than spelled out at each use so that
/// a test can assert that the endpoint and the client agree about them.
const QStringList& clientCodes()
{
    static const QStringList codes = {
        QStringLiteral("invalid_message"), QStringLiteral("too_large"), QStringLiteral("not_connected"),
        QStringLiteral("unsupported_message"), QStringLiteral("too_many_clients"),
        QStringLiteral("render_unavailable"), QStringLiteral("internal_error")
    };
    return codes;
}

}   // namespace

/******************************************************************************
* Constructor.
******************************************************************************/
AutomationLocalClient::AutomationLocalClient(QObject* parent) : QObject(parent) {}

/******************************************************************************
* Destructor.
******************************************************************************/
AutomationLocalClient::~AutomationLocalClient()
{
    disconnectFromEndpoint();
}

/******************************************************************************
* The transport-level codes the client knows.
******************************************************************************/
QStringList AutomationLocalClient::expectedTransportErrorCodes()
{
    return clientCodes();
}

/******************************************************************************
* The sessions a client can see.
******************************************************************************/
QVector<AutomationSessionDescriptor> AutomationLocalClient::discover(QStringList* problems)
{
    return AutomationSessionDescriptor::discover(problems);
}

/******************************************************************************
* Removes the descriptors whose process is gone.
******************************************************************************/
int AutomationLocalClient::pruneStale(QStringList* removed)
{
    int count = 0;
    for(const AutomationSessionDescriptor& descriptor : AutomationSessionDescriptor::discover()) {
        if(!descriptor.isStale())
            continue;
        if(descriptor.removeFromDisk()) {
            count++;
            if(removed)
                removed->push_back(descriptor.sessionId());
        }
    }
    return count;
}

/******************************************************************************
* Connects to an endpoint.
******************************************************************************/
bool AutomationLocalClient::connectToEndpoint(const QString& endpointName, QString* error)
{
    disconnectFromEndpoint();

    _socket = new QLocalSocket(this);
    connect(_socket, &QLocalSocket::readyRead, this, [this]() {
        _buffer += _socket->readAll();
        while(true) {
            const int newline = _buffer.indexOf('\n');
            if(newline < 0)
                return;
            const QByteArray line = _buffer.left(newline);
            _buffer.remove(0, newline + 1);
            if(!line.trimmed().isEmpty())
                handleLine(line);
        }
    });

    _socket->connectToServer(endpointName);
    if(!_socket->waitForConnected(5000)) {
        if(error)
            *error = QStringLiteral("cannot connect to '%1': %2").arg(endpointName, _socket->errorString());
        _socket->deleteLater();
        _socket = nullptr;
        return false;
    }
    _endpointName = endpointName;
    return true;
}

/******************************************************************************
* Connects to the newest discovered session.
******************************************************************************/
bool AutomationLocalClient::connectToDiscoveredSession(const QString& clientName, QString* error)
{
    Q_UNUSED(clientName);
    QStringList problems;
    const QVector<AutomationSessionDescriptor> sessions = discover(&problems);
    if(sessions.isEmpty()) {
        if(error)
            *error = problems.isEmpty() ? QStringLiteral("no session was discovered")
                                        : QStringLiteral("no session was discovered (%1 unusable files)").arg(problems.size());
        return false;
    }
    // Newest first, and the first one that answers wins: a leftover descriptor of a session that crashed is skipped by
    // the connection attempt itself, which is the only reliable test of whether an endpoint is there.
    QStringList failures;
    for(const AutomationSessionDescriptor& session : sessions) {
        QString connectError;
        if(connectToEndpoint(session.endpoint(), &connectError))
            return true;
        failures.push_back(QStringLiteral("%1: %2").arg(session.sessionId(), connectError));
    }
    if(error)
        *error = QStringLiteral("none of the %1 discovered sessions answered (%2)")
                     .arg(sessions.size()).arg(failures.join(QStringLiteral("; ")));
    return false;
}

/******************************************************************************
* Ends the connection.
******************************************************************************/
void AutomationLocalClient::disconnectFromEndpoint()
{
    if(_socket) {
        if(_socket->state() == QLocalSocket::ConnectedState) {
            _socket->flush();
            _socket->disconnectFromServer();
            _socket->waitForDisconnected(1000);
        }
        _socket->deleteLater();
        _socket = nullptr;
    }
    _buffer.clear();
    _pendingReplies.clear();
    _endpointName.clear();
    _clientId.clear();
}

bool AutomationLocalClient::isConnected() const
{
    return _socket && _socket->state() == QLocalSocket::ConnectedState;
}

/******************************************************************************
* Routes one message from the endpoint.
******************************************************************************/
void AutomationLocalClient::handleLine(const QByteArray& line)
{
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if(!document.isObject()) {
        // A line the client cannot parse is kept as it arrived, so a report shows what the endpoint sent rather
        // than claiming it sent nothing.
        _pushed.push_back(QVariantMap{{ QStringLiteral("_unparsed"), QString::fromUtf8(line) }});
        return;
    }
    const QVariantMap message = document.object().toVariantMap();
    if(message.value(QStringLiteral("type")).toString() == QStringLiteral("event")) {
        const QVariantMap event = message.value(QStringLiteral("event")).toMap();
        _pushed.push_back(event);
        Q_EMIT eventReceived(event);
        return;
    }
    if(message.contains(QStringLiteral("id")))
        _pendingReplies.insert(message.value(QStringLiteral("id")).toULongLong(), message);
    else if(message.value(QStringLiteral("type")).toString() == QStringLiteral("reply"))
        // A reply without an ID is the endpoint answering something it could not match to a request: bytes that are not
        // a message, or a connection refused before the handshake. It is kept apart from the pushed events so that a
        // caller which sent such bytes still receives its answer.
        _unmatchedReplies.push_back(message);
    else
        _pushed.push_back(message);
}

/******************************************************************************
* Sends one message and waits for its reply.
******************************************************************************/
AutomationLocalClient::Reply AutomationLocalClient::exchange(const QVariantMap& message, int timeoutMs)
{
    Reply reply;
    if(!isConnected()) {
        reply.errorCode = QStringLiteral("not_connected");
        reply.errorMessage = QStringLiteral("the client is not connected to an endpoint");
        return reply;
    }

    QVariantMap outgoing = message;
    const quint64 id = _nextMessageId++;
    outgoing.insert(QStringLiteral("id"), QVariant::fromValue<qulonglong>(id));
    _socket->write(QJsonDocument(QJsonObject::fromVariantMap(outgoing)).toJson(QJsonDocument::Compact) + '\n');
    _socket->flush();

    QElapsedTimer timer;
    timer.start();
    while(true) {
        if(_pendingReplies.contains(id)) {
            const QVariantMap answer = _pendingReplies.take(id);
            Reply parsed = replyToReply(answer);
            parsed.answered = true;
            return parsed;
        }
        // An endpoint that refused this connection answers without an ID - it never saw a request of ours - and then
        // closes the socket. That answer is the result of this exchange, not a timeout.
        if(!_unmatchedReplies.isEmpty()) {
            Reply parsed = replyToReply(_unmatchedReplies.takeFirst());
            parsed.answered = true;
            return parsed;
        }
        const int remaining = timeoutMs - int(timer.elapsed());
        if(remaining <= 0 || !_socket->waitForReadyRead(remaining))
            break;
    }
    reply.errorCode = QStringLiteral("timeout");
    reply.errorMessage = QStringLiteral("the endpoint did not answer within %1 ms").arg(timeoutMs);
    return reply;
}

/******************************************************************************
* Parses the envelope of a reply.
******************************************************************************/
AutomationLocalClient::Reply AutomationLocalClient::replyToReply(const QVariantMap& message) const
{
    Reply reply;
    reply.transportOk = message.value(QStringLiteral("ok")).toBool();
    if(reply.transportOk) {
        reply.result = message.value(QStringLiteral("result")).toMap();
    }
    else {
        const QVariantMap error = message.value(QStringLiteral("error")).toMap();
        reply.errorCode = error.value(QStringLiteral("code")).toString();
        reply.errorMessage = error.value(QStringLiteral("message")).toString();
        reply.errorDetails = error.value(QStringLiteral("details")).toMap();
    }
    return reply;
}

/******************************************************************************
* The handshake.
******************************************************************************/
AutomationLocalClient::Reply AutomationLocalClient::hello(const QString& clientName, const QStringList& requestedCapabilities,
                                                         const QString& origin, const QString& protocol)
{
    QVariantMap message;
    message.insert(QStringLiteral("type"), QStringLiteral("hello"));
    message.insert(QStringLiteral("client"), clientName);
    message.insert(QStringLiteral("origin"), origin);
    message.insert(QStringLiteral("protocol"), protocol.isEmpty() ? AutomationContract::version() : protocol);
    QVariantList capabilities;
    for(const QString& capability : requestedCapabilities)
        capabilities.push_back(capability);
    message.insert(QStringLiteral("capabilities"), capabilities);
    Reply reply = exchange(message, 10000);
    if(reply.answered && reply.transportOk)
        _clientId = reply.result.value(QStringLiteral("clientId")).toString();
    return reply;
}

/******************************************************************************
* The remaining messages.
******************************************************************************/
AutomationLocalClient::Reply AutomationLocalClient::ping()
{
    return exchange(QVariantMap{{ QStringLiteral("type"), QStringLiteral("ping") }}, 5000);
}

AutomationLocalClient::Reply AutomationLocalClient::dispatch(const QString& operation, const QVariantMap& arguments,
                                                            std::optional<quint64> baseRevision)
{
    QVariantMap message;
    message.insert(QStringLiteral("type"), QStringLiteral("dispatch"));
    message.insert(QStringLiteral("operation"), operation);
    message.insert(QStringLiteral("arguments"), arguments);
    if(baseRevision)
        message.insert(QStringLiteral("baseRevision"), QVariant::fromValue<qulonglong>(*baseRevision));
    return exchange(message, 30000);
}

AutomationLocalClient::Reply AutomationLocalClient::snapshot(int maximumNodes)
{
    QVariantMap message;
    message.insert(QStringLiteral("type"), QStringLiteral("snapshot"));
    if(maximumNodes > 0)
        message.insert(QStringLiteral("maxNodes"), maximumNodes);
    return exchange(message, 30000);
}

AutomationLocalClient::Reply AutomationLocalClient::subscribe(quint64 since)
{
    QVariantMap message;
    message.insert(QStringLiteral("type"), QStringLiteral("subscribe"));
    message.insert(QStringLiteral("since"), QVariant::fromValue<qulonglong>(since));
    return exchange(message, 10000);
}

AutomationLocalClient::Reply AutomationLocalClient::unsubscribe()
{
    return exchange(QVariantMap{{ QStringLiteral("type"), QStringLiteral("unsubscribe") }}, 10000);
}

AutomationLocalClient::Reply AutomationLocalClient::capture(const QSize& size)
{
    QVariantMap message;
    message.insert(QStringLiteral("type"), QStringLiteral("capture"));
    message.insert(QStringLiteral("width"), size.width());
    message.insert(QStringLiteral("height"), size.height());
    return exchange(message, 30000);
}

AutomationLocalClient::Reply AutomationLocalClient::quit()
{
    return exchange(QVariantMap{{ QStringLiteral("type"), QStringLiteral("quit") }}, 5000);
}

AutomationLocalClient::Reply AutomationLocalClient::sendRawLine(const QByteArray& line)
{
    if(!isConnected()) {
        Reply reply;
        reply.errorCode = QStringLiteral("not_connected");
        return reply;
    }
    // The malformed-request case is about what the endpoint does with bytes that are not a message, so the client sends
    // them verbatim - without an ID it cannot be matched, which is why the answer is read from the pending queue.
    _socket->write(line);
    _socket->write("\n");
    _socket->flush();

    QElapsedTimer timer;
    timer.start();
    while(_pendingReplies.isEmpty() && _unmatchedReplies.isEmpty()) {
        const int remaining = 5000 - int(timer.elapsed());
        if(remaining <= 0 || !_socket->waitForReadyRead(remaining)) {
            Reply reply;
            reply.errorCode = QStringLiteral("timeout");
            return reply;
        }
    }
    Reply reply = replyToReply(_unmatchedReplies.isEmpty() ? takePendingReply() : _unmatchedReplies.takeFirst());
    reply.answered = true;
    return reply;
}

/******************************************************************************
* Takes the oldest unclaimed reply.
******************************************************************************/
QVariantMap AutomationLocalClient::takePendingReply()
{
    if(_pendingReplies.isEmpty())
        return {};
    const qulonglong key = _pendingReplies.keys().front();
    return _pendingReplies.take(key);
}

/******************************************************************************
* Receives one message, whether a reply or a pushed event.
******************************************************************************/
bool AutomationLocalClient::receiveMessage(int timeoutMs)
{
    if(!isConnected())
        return false;
    QElapsedTimer timer;
    timer.start();
    while(true) {
        if(!_pendingReplies.isEmpty() || !_pushed.isEmpty())
            return true;
        const int remaining = timeoutMs - int(timer.elapsed());
        if(remaining <= 0)
            return false;
        if(!_socket->waitForReadyRead(remaining))
            return !_pendingReplies.isEmpty() || !_pushed.isEmpty();
    }
}

/******************************************************************************
* The pushed events, in arrival order.
******************************************************************************/
QVector<QVariantMap> AutomationLocalClient::pushedEvents() const
{
    QVector<QVariantMap> events;
    for(const QVariantMap& message : _pushed) {
        if(message.contains(QStringLiteral("kind")))
            events.push_back(message);
    }
    return events;
}

}   // namespace Ovito
