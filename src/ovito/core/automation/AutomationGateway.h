// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationContract.h>
#include <ovito/core/automation/AutomationProtocol.h>

#include <QHash>

#include <functional>

namespace Ovito {

class AutomationSession;
class AutomationTaskRecord;
class AutomationTransaction;

/**
 * \brief The capabilities one automation client has been granted.
 *
 * A client starts with the read capabilities and nothing else: it can inspect the session, the scene and the
 * selection, which is what an agent needs to understand a workbench before it is allowed to touch anything. Every
 * capability that can change something - including executing Python and writing files - is granted explicitly.
 *
 * This class is the data of the model. Who grants a capability, and what the grant leaves behind in the session's
 * history, is the gateway's business: AutomationGateway::grantCapability() records the grant as an activity, which is
 * what an audit of "who allowed this" reads. A frontend that only reads the set uses permissions().
 *
 * The scope of a grant is the life of the client, i.e. of its gateway. Nothing here is persisted, and no operation can
 * grant a capability: an operation has a descriptor and a handler, and a descriptor that required, say, FileWrite in
 * order to hand it out would make the client's own authority the source of the grant. Persistence and confirmation at
 * the user interface belong to the phases that add a transport and a permission dialog.
 */
class OVITO_CORE_EXPORT AutomationPermissionSet
{
public:

    /// The capabilities any client has after connecting: read-only, which is the documented default of the design.
    static QVector<AutomationContract::Capability> readOnlyDefaults();

    /// Starts with the read-only defaults. Pass false for a client that starts with nothing at all, for example
    /// before the user has approved the connection.
    explicit AutomationPermissionSet(bool grantReadOnlyDefaults = true);

    bool contains(AutomationContract::Capability capability) const { return _capabilities.contains(static_cast<int>(capability)); }

    /// Grants one capability. A capability that permits a change brings TaskControl with it - whoever may start work
    /// may stop it again; see the class comment - while a read capability implies nothing.
    void grant(AutomationContract::Capability capability) { insert(capability); }
    void grant(const QVector<AutomationContract::Capability>& capabilities);
    /// Grants every read capability, which is what a client that has just been approved gets; see readOnlyDefaults().
    void grantReadOnlyDefaults() { grant(readOnlyDefaults()); }
    void revoke(AutomationContract::Capability capability) { _capabilities.remove(static_cast<int>(capability)); }
    void clear() { _capabilities.clear(); }

    /// The granted capabilities in wire-name form, sorted, as a client sees them.
    QStringList names() const;

private:

    /// Adds one capability and, for the mutating ones, the TaskControl they imply.
    void insert(AutomationContract::Capability capability);

    /// The granted capabilities as their enum values. The set is private and the enum has no qHash that QSet could
    /// use, so the conversion happens here at the boundary and the methods above stay typed.
    QSet<int> _capabilities;
};

/**
 * \brief The operation catalog, the capability checks and the revision preconditions of the automation boundary.
 *
 * A gateway sits between one client and one AutomationSession. It is the only way to reach a domain operation through
 * automation, and it is deliberately the same object for a command line client, an AI agent, a later MCP adapter, a
 * Python bridge and - where a frontend panel wants it - a QML panel. None of them implements a mutation: they look an
 * operation up in the catalog, hand over arguments, and read a structured result.
 *
 * A gateway is one client: it has an ID of its own, the origin its activity is attributed to, and its own capabilities.
 *
 * What a request goes through, in this order:
 *
 *  1. the operation ID resolves to a descriptor (else UnknownOperation, with the catalog in the details),
 *  2. the descriptor has an implementation in this build (else NotSupported),
 *  3. the arguments match the parameter schema (else InvalidArgument, with one message per problem),
 *  4. the client holds every capability the descriptor requires (else MissingCapability, naming the missing ones),
 *  5. the base revision, if the client sent one, is still current (else StaleRevision, with the current revision),
 *  6. a command opens a task and joins a transaction boundary (its own, or the open one of the session); the request is
 *     recorded as an activity of this session, with its operation, client, origin and task,
 *  7. the operation runs; an exception becomes a structured error instead of reaching the client as a crash,
 *  8. a command that succeeded commits its boundary and advances the session revision; a command that failed rolls its
 *     own changes back, so an operation that reports a failure leaves no half-applied change behind,
 *  9. the task ends in the state its result implies and keeps that result.
 *
 * The gateway is the shared controller the design asks for: `core domain operation -> automation catalog/controller ->
 * QML / QtWidgets / CLI / AI or MCP adapter`. The frontends keep their `Command` objects as presentation, and a
 * command's handler may call into the gateway, but the gateway never owns a second copy of a domain operation.
 */
class OVITO_CORE_EXPORT AutomationGateway : public QObject
{
public:

    /// What an operation does once the request has passed the checks. The result already carries the contract version
    /// and the revision, so a handler only fills in the payload, the warnings and the IDs it created.
    ///
    /// A handler answers in place: it writes its payload into the result, and an error it cannot answer through with
    /// `AutomationResult::setError()`. A handler must not return a result of its own - the gateway discards it, the
    /// same way `std::function<void(...)>` accepts any callable whose return value is ignored.
    ///
    /// A handler that runs long reports through this_automation_task::progress() and stops when
    /// this_automation_task::isCancellationRequested() says so; a command runs with a task, a query does not.
    using OperationHandler = std::function<void(const AutomationRequest& request, AutomationResult& result)>;

    /// A gateway for a client whose activity is the user's - a frontend, a test, an embedded client.
    explicit AutomationGateway(AutomationSession& session, QObject* parent = nullptr);

    /// A gateway for a client of a kind: a command line client, an AI agent, a Python bridge. The session gives it the
    /// ID it is known by, and the name is for the log and for a user interface that shows who is connected.
    AutomationGateway(AutomationSession& session, AutomationContract::ActivityOrigin origin, QString clientName = {},
                      QObject* parent = nullptr);

    ~AutomationGateway() override;

    AutomationGateway(const AutomationGateway&) = delete;
    AutomationGateway& operator=(const AutomationGateway&) = delete;
    AutomationGateway(AutomationGateway&&) = delete;
    AutomationGateway& operator=(AutomationGateway&&) = delete;

    /// The session this gateway describes and operates on.
    AutomationSession& session() const { return _session; }

    /// The ID this client is known by in the session, `client:c1`.
    const QString& clientId() const { return _clientId; }

    /// The origin this client's activity is attributed to.
    AutomationContract::ActivityOrigin origin() const { return _origin; }

    /// The capabilities of this client; reading them is how a frontend shows what an agent may do.
    const AutomationPermissionSet& permissions() const { return _permissions; }

    /**
     * \brief Grants a capability and records the grant as an activity of the session.
     *
     * This is the path a frontend or an embedder takes after the user allowed the client to do something. The grant
     * itself is what happens; the activity is what makes it answerable later, which is why the direct accessors of
     * AutomationPermissionSet are not the path a user interface takes.
     */
    void grantCapability(AutomationContract::Capability capability);
    void grantCapabilities(const QVector<AutomationContract::Capability>& capabilities);
    void grantReadOnlyDefaults();
    void revokeCapability(AutomationContract::Capability capability);
    void clearCapabilities();

    /**
     * \brief Adds an operation to the catalog.
     * \param descriptor What the operation is, needs and accepts. Its ID must be new.
     * \param handler What it does; an empty handler declares the operation without implementing it, which makes the
     *                gateway answer NotSupported. That is how an operation the design assigns to a later phase is
     *                announced without pretending to work.
     */
    void registerOperation(AutomationOperationDescriptor descriptor, OperationHandler handler = {});

    /// The whole catalog, sorted by operation ID, which is the form a client enumerates.
    QVector<AutomationOperationDescriptor> operations() const;

    /// Looks an operation up by ID.
    std::optional<AutomationOperationDescriptor> findOperation(const QString& operationId) const;

    /// Reports whether an operation is declared.
    bool hasOperation(const QString& operationId) const { return _operations.contains(operationId); }

    /// The catalog as JSON, the form a client asks for first.
    QVariantList catalogToJson() const;

    /// Runs one request through the checks above and returns its structured result.
    AutomationResult dispatch(const AutomationRequest& request);

private:

    /// Registers the read-only queries and the task operations this phase provides to prove the boundary; see the
    /// class comment.
    void registerBuiltinOperations();

    /// Records one activity of this client in the session's event log.
    void logActivity(QString summary, QString operationId = {}, QString taskId = {}, QString transactionId = {},
                     QVariantMap details = {});

    struct Entry {
        AutomationOperationDescriptor descriptor;
        OperationHandler handler;
    };

    AutomationSession& _session;
    QString _clientId;
    AutomationContract::ActivityOrigin _origin = AutomationContract::ActivityOrigin::User;
    AutomationPermissionSet _permissions;

    QHash<QString, Entry> _operations;
};

}   // End of namespace
