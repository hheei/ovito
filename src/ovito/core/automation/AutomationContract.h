// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

namespace Ovito {

/**
 * \brief The machine-facing contract that every automation client speaks.
 *
 * OVITO has three kinds of user of its domain operations: the two interactive frontends (QtWidgets and Qt Quick),
 * scripts written against the Python API, and machine clients such as a command line tool, a coding agent or a later
 * MCP adapter. The first kind drives the domain objects directly today. The other two must not: a client that
 * reimplements "insert a modifier" or "set this parameter" produces a second copy of the mutation logic that
 * drifts away from the pipeline, the undo stack and the session semantics.
 *
 * This class and its neighbours in `ovito/core/automation` define the boundary those clients share:
 *
 *  - AutomationContract - the names and versions of the vocabulary (this file),
 *  - AutomationOperationDescriptor, AutomationParameter, AutomationRequest and AutomationResult - the wire types,
 *  - AutomationObjectId and AutomationObjectRegistry - stable identities for scene objects,
 *  - AutomationSession - the state a client observes (data set, revision, object identities),
 *  - AutomationGateway - the operation catalog, the capability checks and the revision preconditions.
 *
 * This is deliberately *not* the `Ovito::Command` type of the frontends: a Command is a presentation object owned by
 * the action manager and mirrored into a QAction or a QML menu entry. A machine client needs a descriptor with a
 * parameter schema, a capability requirement and a structured result, which a Command does not carry. Both describe
 * the same underlying domain operation, and that operation belongs in the gateway.
 *
 * Compatibility rule for the schema in this directory: a new minor version may add operations, parameters, capability
 * names, error codes and result fields, and may relax a previously required parameter. It may not remove or rename
 * any of them, require a new parameter, change a parameter type, or change the meaning of an existing error code;
 * that needs a new major version. Version 0.x means the contract is still being completed by the remaining Phase 2.6
 * deliverables and has one consumer at a time.
 */
class OVITO_CORE_EXPORT AutomationContract
{
public:

    /// Whether an operation only reads the session or also changes it.
    enum class OperationKind {

        /// Inspects the session, the scene, a pipeline or a viewport. Queries never change state and never require a
        /// capability that permits a change, which is what makes a read-only client possible.
        Query,

        /// Changes the session. A command needs the capabilities its descriptor declares, and the gateway reports the
        /// new session revision in its result.
        Command
    };

    /**
     * \brief A named permission an operation can require.
     *
     * The names are part of the contract. The read capabilities form a closed set (see isReadCapability) because a
     * query may only require those; everything else is a mutation, and a client that is not allowed to mutate gets
     * the read capabilities plus whatever the user grants explicitly.
     */
    enum class Capability {

        /// Inspect the session: its revision, its file path, the set of scene nodes and viewports. The only capability
        /// a bare connect needs.
        SessionRead,

        /// Inspect the structure and the parameters of pipelines and their modifiers.
        SceneRead,

        /// Inspect the selection and the current frame.
        SelectionRead,

        /// Read a file chosen by the user, for example data to import.
        FileRead,

        /// Create, delete, reorder or parameterize pipeline nodes.
        PipelineWrite,

        /// Change the selection.
        SelectionWrite,

        /// Change the current frame or the animation playback.
        AnimationWrite,

        /// Execute Python code supplied by a client. Never granted by default; see the design document.
        PythonExecute,

        /// Write a file: save a session, export data, save a rendered image.
        FileWrite,

        /// Open a network connection.
        NetworkAccess,

        /// Start an external process.
        ProcessExecute,

        /// Cancel an automation task this client started. Not part of the read capabilities and not granted by default:
        /// a read-only client cannot start a task, so it never needs to stop one. A client that is granted one of the
        /// work capabilities is granted this one with it; see AutomationPermissionSet.
        TaskControl
    };

    /**
     * \brief The lifecycle state of an automation task.
     *
     * A task is created when an operation is dispatched and it is what a client watches while the operation runs: it
     * carries the progress, the cancellation flag, and finally the result. Started and finished are separate states so
     * that a client can tell "is still running" from "has run and answered"; a task that is cancelled while running
     * ends in Cancelled rather than in Completed, which is what a caller waiting for it needs to see.
     */
    enum class TaskState {

        /// Created, not yet running: a state that only exists between the dispatch of a request and its execution.
        Pending,

        /// The operation is executing.
        Running,

        /// The operation returned a successful result.
        Completed,

        /// The operation failed: the task carries the error code and message of its result.
        Failed,

        /// The operation was canceled, either because the client asked for it or because the surrounding work was.
        Cancelled
    };

    /**
     * \brief Where an activity in the session came from.
     *
     * Every event and every task names one of these, so that a later client - an AI agent reading the recent history,
     * or a Python script explaining a change - can tell a user's edit from its own. The list is deliberately short: an
     * origin is a category, not an identity (a client's identity is its own ID).
     */
    enum class ActivityOrigin {

        /// The user at a frontend: a QtWidgets dialog, a drag in the viewport, a QML panel that is not a machine client.
        User,

        /// The Qt Quick frontend acting through the shared layer rather than through a machine client.
        Qml,

        /// A command line client.
        Cli,

        /// An AI agent or a tool that plans on the user's behalf.
        Ai,

        /// Code executing in a Python runtime that is attached to this session.
        Python
    };

    /// What one entry of the session's event log is about. A client switches on this, not on a numeric code.
    enum class EventKind {

        /// The session revision advanced: another data set became current, a viewport, the selection or the current
        /// frame changed, or a command ran.
        SessionChanged,

        /// An automation task was created for an operation.
        TaskStarted,

        /// A running task reported progress.
        TaskProgress,

        /// A task reached a terminal state: completed, failed or cancelled.
        TaskFinished,

        /// A semantic activity: a client connected, a capability was granted or revoked, a transaction was opened,
        /// committed or aborted, or a handler recorded one of its own. Carries no raw input and no data-derived value
        /// unless the recorder put one there on purpose.
        Activity
    };

    /**
     * \brief The state of a transaction boundary.
     *
     * A transaction is the unit in which automation changes become undoable: the commands it collected are one step on
     * the undo stack, and a failure inside it takes them all back. A client sees the state of the transaction its
     * request reports, which tells it whether its change is in effect (Committed) rather than having been rolled back
     * with the rest of a failed operation (Aborted).
     */
    enum class TransactionState {

        /// Collecting commands; nothing is on the undo stack yet.
        Open,

        /// Finished successfully: the changes are in effect and, where the session has an undo stack, one undo step.
        Committed,

        /// Rolled back: the changes the boundary collected were undone and nothing was written to the undo stack.
        Aborted
    };

    /// A machine-readable failure classification. A client switches on this, never on the message text.
    enum class ErrorCode {

        /// The request is not well formed: no operation ID, an unparsable base revision.
        InvalidRequest,

        /// The gateway has no operation with that ID. The result lists the operations it does have.
        UnknownOperation,

        /// The operation exists but the arguments do not match its parameter schema.
        InvalidArgument,

        /// The client lacks a capability the descriptor requires. The result lists the missing ones.
        MissingCapability,

        /// The request carried a base revision that is older than the session revision; the client has to re-query.
        StaleRevision,

        /// The request names an object the session does not know.
        UnknownObject,

        /// The request names an object that existed when the client looked, but its identity has been released: the
        /// data set was replaced, or the object was deleted and not resurrected by the undo stack.
        InvalidatedObject,

        /// The operation is declared but not implemented in this build or at this phase.
        NotSupported,

        /// The operation was cancelled, either by the user or by the owning task.
        Cancelled,

        /// Anything else, including an OVITO exception that escaped the operation.
        InternalError
    };

    /// Major version of the contract; a different major version means an incompatible change. See the class comment.
    static constexpr int versionMajor = 0;

    /// Minor version of the contract; see the class comment for what a new minor version may and may not do.
    /// 0.2 added the task lifecycle vocabulary (TaskState, EventKind, ActivityOrigin) and the capability TaskControl.
    static constexpr int versionMinor = 2;

    /// Returns the contract version as `"<major>.<minor>"`, the value every result carries.
    static QString version();

    /// Returns the wire name of an operation kind, `"query"` or `"command"`.
    static QString kindName(OperationKind kind);

    /// Returns the wire name of a capability, for example `"session.read"`.
    static QString capabilityName(Capability capability);

    /// Looks a capability up by its wire name; nothing if the name is unknown.
    static std::optional<Capability> capabilityFromName(QStringView name);

    /// Returns whether a capability only permits reading. A query descriptor may require read capabilities only.
    static bool isReadCapability(Capability capability);

    /// Returns the wire name of an error code, for example `"stale_revision"`.
    static QString errorCodeName(ErrorCode code);

    /// Returns the wire name of a task state, for example `"running"`.
    static QString taskStateName(TaskState state);

    /// Returns whether a task state is final, i.e. the task will not change again.
    static bool isTerminalTaskState(TaskState state);

    /// Returns the wire name of an activity origin, for example `"ai"`.
    static QString originName(ActivityOrigin origin);

    /// Looks an activity origin up by its wire name; nothing if the name is unknown.
    static std::optional<ActivityOrigin> originFromName(QStringView name);

    /// Returns the wire name of an event kind, for example `"task.finished"`.
    static QString eventKindName(EventKind kind);

    /// Returns the wire name of a transaction state, for example `"committed"`.
    static QString transactionStateName(TransactionState state);

    /// Looks an event kind up by its wire name; nothing if the name is unknown.
    static std::optional<EventKind> eventKindFromName(QStringView name);
};

}   // End of namespace
