// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationGateway.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>

#include <QImage>
#include <QLocalServer>
#include <QObject>
#include <QVariantMap>

#include <functional>

namespace Ovito {

class AutomationLocalClient;

/**
 * \brief The spike's local-only endpoint: one workbench, one local socket, one JSON object per line.
 *
 * This is the measurable half of deliverable 8. It is *not* the production transport - the endpoint is a spike-local
 * class inside `automation/spike/`, and Phase 3 owns the real one - but it is a real endpoint in the sense that
 * matters: it lets a client discover a session on a filesystem convention, connect to it without a network listener
 * existing anywhere, read a bounded snapshot of the session, receive task and scene events, and ask for an image
 * without any file being written.
 *
 * The design decisions the endpoint demonstrates, each of which Phase 3 can keep or replace:
 *
 * - **Local only, by construction.** The transport is a QLocalServer, i.e. a named pipe on Windows and a UNIX domain
 *   socket elsewhere, in a directory that only its owner can enter. There is no TCP listener and no way to configure
 *   one, so "network listening is disabled by default" is not a setting that can be wrong.
 * - **JSON Lines of control messages.** One JSON object per line, for the messages and the small values the contract
 *   already defines. Scientific arrays never travel this way - that is the rule D49 fixed, and the endpoint does not
 *   weaken it: it has no array transport at all, and the one artifact it can return is bounded and in memory.
 * - **Capabilities per connection, read-only by default.** Every connection gets its own AutomationGateway, so it has
 *   its own identity, its own capability set and its own activity in the session's log. The client asks for what it
 *   wants in its hello message; the endpoint grants what its policy allows and answers with the grant *and* the
 *   refusal, because a client that is silently given less than it asked for is a client that will misreport.
 * - **Two layers of errors.** A reply is either a transport failure (`ok: false`, a transport code such as
 *   `invalid_message`) or a successful exchange carrying the contract's own result, whose `ok` may still be false with
 *   an error code of the contract vocabulary. The distinction is what lets a client tell "you did not understand me"
 *   from "the workbench refused the operation".
 *
 * The endpoint holds no reference to a frontend: it works on an AutomationSession and a DataSet, which is why it can be
 * exercised in a process without a window (see IpcSpikeMain.cpp) and why a later endpoint can be moved into the
 * frontend without changing what a client sees.
 */
class AutomationLocalEndpoint : public QObject
{
    Q_OBJECT

public:

    /// What the endpoint refuses to do, in numbers a test can assert.
    struct Limits {
        /// The longest request line accepted; a longer line is answered with `too_large` and closes the connection.
        qint64 maximumRequestBytes = 64 * 1024;
        /// The number of simultaneous connections.
        int maximumClients = 4;
        /// The number of scene nodes a snapshot describes before it sets its truncation flag.
        int maximumSnapshotNodes = 200;
        /// The number of retained events one reply carries.
        int maximumEvents = 256;
        /// The largest image a client may ask for.
        QSize maximumCaptureSize = QSize(4096, 4096);
        /// The largest encoded image artifact, in bytes.
        qint64 maximumArtifactBytes = 4 * 1024 * 1024;
    };

    /// Produces the image of a capture request, or an error message. An endpoint without a source answers
    /// `render_unavailable`, which is the honest answer of a process that has no graphics device.
    using CaptureSource = std::function<QImage(QSize size, QString* error)>;

    /// The counters the spike reports; they are also what a Phase 3 endpoint would expose as its own observability.
    struct Statistics {
        qint64 connections = 0;
        qint64 rejectedConnections = 0;
        qint64 requests = 0;
        qint64 transportErrors = 0;
        qint64 contractErrors = 0;
        qint64 eventsPushed = 0;
        qint64 artifacts = 0;
    };

    AutomationLocalEndpoint(AutomationSession& session, QObject* parent = nullptr);
    ~AutomationLocalEndpoint() override;

    AutomationLocalEndpoint(const AutomationLocalEndpoint&) = delete;
    AutomationLocalEndpoint& operator=(const AutomationLocalEndpoint&) = delete;

    /// The most a connecting client can be granted. A spike run passes the read capabilities plus whatever its command
    /// line approved, which is a stand-in for the user's consent (D43).
    void setGrantPolicy(const AutomationPermissionSet& policy);
    const AutomationPermissionSet& grantPolicy() const { return _policy; }

    void setLimits(const Limits& limits) { _limits = limits; }
    const Limits& limits() const { return _limits; }

    /// Sets the image source; without one every capture answers `render_unavailable`.
    void setCaptureSource(CaptureSource source) { _captureSource = std::move(source); }

    /// Starts listening, publishes the session descriptor and returns false with a reason when either fails.
    bool start(QString* error = nullptr);

    /// Closes the endpoint, disconnects its clients and removes the descriptor. Safe to call twice.
    void stop();

    bool isListening() const { return _server.isListening(); }
    int connectedClients() const { return _clients.size(); }
    const QString& endpointName() const { return _descriptor.endpoint(); }
    const AutomationSessionDescriptor& descriptor() const { return _descriptor; }
    const Statistics& statistics() const { return _statistics; }

    /// The transport-level error codes, which are the ones a client can rely on independently of the contract.
    static QStringList transportErrorCodes();

    /// True if the endpoint may answer `quit`; a client that can stop the workbench is a client the spike's own
    /// self-test creates, not a capability of the protocol.
    void setAllowsQuit(bool allows) { _allowsQuit = allows; }

Q_SIGNALS:

    void clientConnected(const QString& clientId, const QString& clientName);
    void clientDisconnected(const QString& clientId);
    void requestHandled(const QString& type, bool transportOk);
    /// Emitted when the client asked the endpoint to stop, which the endpoint only accepts when setAllowsQuit() is set.
    void quitRequested();

private Q_SLOTS:

    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onEventAppended(quint64 sequence);

private:

    /// One connected client: its socket, its identity and its own gateway with its own capabilities.
    struct Client {
        QLocalSocket* socket = nullptr;
        QString id;
        QString name;
        bool greeted = false;
        bool subscribed = false;
        quint64 lastEventSequence = 0;
        QByteArray buffer;
        AutomationGateway* gateway = nullptr;
    };

    Client* clientFor(const QLocalSocket* socket) const;
    void serveLine(Client& client, const QByteArray& line);
    void handleHello(Client& client, const QVariantMap& message, QVariantMap& reply);
    void handleDispatch(Client& client, const QVariantMap& message, QVariantMap& reply);
    void handleSnapshot(Client& client, const QVariantMap& message, QVariantMap& reply);
    void handleSubscribe(Client& client, const QVariantMap& message, QVariantMap& reply);
    void handleCapture(Client& client, const QVariantMap& message, QVariantMap& reply);
    void sendReply(Client& client, const QVariantMap& reply);
    void sendError(Client& client, const QVariant& requestId, const QString& code, const QString& message,
                   QVariantMap details = {});
    QVariantMap makeReply(const QVariant& requestId) const;
    void closeClient(Client& client, const QString& reason);

    AutomationSession& _session;
    QLocalServer _server;
    AutomationSessionDescriptor _descriptor;
    std::vector<std::unique_ptr<Client>> _clients;
    QMetaObject::Connection _eventConnection;
    AutomationPermissionSet _policy;
    Limits _limits;
    CaptureSource _captureSource;
    Statistics _statistics;
    bool _allowsQuit = false;
};

}   // namespace Ovito
