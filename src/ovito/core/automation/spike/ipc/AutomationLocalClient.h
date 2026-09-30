// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QSize>
#include <QVariantMap>

namespace Ovito {

/**
 * \brief The spike's client of the local endpoint: discovery, one JSON object per line, request and reply by ID.
 *
 * Like the endpoint itself this is spike code (the production client is Phase 3's, and the CLI that uses it is
 * Phase 4's), and like the endpoint it is written so that the protocol rules it depends on are visible in one place:
 * it discovers a session through the descriptor directory, it connects to the endpoint the descriptor names, it
 * identifies itself once, and from then on it sends one message at a time and matches the reply by its ID.
 *
 * Discovery is where a client has to be careful, and this one is deliberately explicit about it: a descriptor whose
 * process is gone is a leftover, and a descriptor whose socket refuses a connection is a leftover too - the first is
 * visible in the file, the second only in the attempt. The client reports both, removes neither by itself unless asked
 * (pruneStale), because deleting another user's discovery entries is not a decision a connecting client gets to make.
 */
class AutomationLocalClient : public QObject
{
    Q_OBJECT

public:

    /// What one exchange with the endpoint produced.
    struct Reply {
        bool transportOk = false;      ///< The endpoint understood the message.
        QString errorCode;             ///< The transport-level code when it did not.
        QString errorMessage;
        QVariantMap errorDetails;
        QVariantMap result;            ///< The payload: an embedded contract result, a snapshot, an artifact.
        bool answered = false;         ///< A matching reply arrived before the timeout.
    };

    explicit AutomationLocalClient(QObject* parent = nullptr);
    ~AutomationLocalClient() override;

    AutomationLocalClient(const AutomationLocalClient&) = delete;
    AutomationLocalClient& operator=(const AutomationLocalClient&) = delete;

    /// The sessions a client can see, newest first, with the files that are not usable descriptors reported separately.
    static QVector<AutomationSessionDescriptor> discover(QStringList* problems = nullptr);

    /// Removes the descriptors whose process is gone, and reports which ones they were.
    static int pruneStale(QStringList* removed = nullptr);

    /// Connects to an endpoint name. Returns false and sets describeError on a socket or handshake failure.
    bool connectToEndpoint(const QString& endpointName, QString* error = nullptr);

    /// Connects to the newest discovered session, after the stale-entry rules above. `clientName` is what the session
    /// records as the connected client.
    bool connectToDiscoveredSession(const QString& clientName, QString* error = nullptr);

    /// Sends the handshake and reads its answer; the granted and refused capabilities are in the reply's result.
    Reply hello(const QString& clientName, const QStringList& requestedCapabilities, const QString& origin = QStringLiteral("cli"),
                const QString& protocol = {});

    Reply ping();
    Reply dispatch(const QString& operation, const QVariantMap& arguments = {}, std::optional<quint64> baseRevision = std::nullopt);
    Reply snapshot(int maximumNodes = 0);
    Reply subscribe(quint64 since = 0);
    Reply unsubscribe();
    Reply capture(const QSize& size);
    Reply quit();
    Reply sendRawLine(const QByteArray& line);
    void disconnectFromEndpoint();

    bool isConnected() const;
    const QString& clientId() const { return _clientId; }
    const QString& endpointName() const { return _endpointName; }

    /// Receives one message, either a reply or a pushed event. Returns false when nothing arrived within `timeoutMs`.
    bool receiveMessage(int timeoutMs = 5000);

    /// The messages the endpoint pushed, in arrival order.
    const QVector<QVariantMap>& pushedMessages() const { return _pushed; }
    QVector<QVariantMap> pushedEvents() const;

    /// The transport-level error codes the client expects to be able to tell apart.
    static QStringList expectedTransportErrorCodes();

Q_SIGNALS:

    void eventReceived(const QVariantMap& event);

private:

    void handleLine(const QByteArray& line);
    Reply exchange(const QVariantMap& message, int timeoutMs = 10000);
    Reply replyToReply(const QVariantMap& message) const;
    QVariantMap takePendingReply();

    QLocalSocket* _socket = nullptr;
    QByteArray _buffer;
    QVector<QVariantMap> _pushed;
    /// Replies the endpoint could not match to a request: an oversized line, or a connection refused before the
    /// handshake. They are kept apart from the pushed events so that a caller which sent bytes the endpoint could not
    /// parse still receives its answer.
    QVector<QVariantMap> _unmatchedReplies;
    QHash<qulonglong, QVariantMap> _pendingReplies;
    QString _endpointName;
    QString _clientId;
    quint64 _nextMessageId = 1;
};

}   // namespace Ovito
