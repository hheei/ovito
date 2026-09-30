// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationEvent.h>
#include <ovito/core/automation/AutomationProtocol.h>

#include <QDateTime>

#include <list>
#include <memory>

namespace Ovito {

class AutomationTaskRegistry;
class Task;

/// The helpers a running operation reports through; they are friends of the task record, which is the only object they
/// change. Declared here so that the record can name them, defined below with the rest of the namespace.
namespace this_automation_task {
OVITO_CORE_EXPORT void progress(double fraction, QString text);
OVITO_CORE_EXPORT void recordActivity(QString summary, QVariantMap details);
}   // namespace this_automation_task

/**
 * \brief One automation task: an operation a client asked for, from its start to its answer.
 *
 * A task is what makes the lifecycle of a request visible: it is created when a command is dispatched, it reports
 * progress while the command runs, it can be asked to stop, and it ends in exactly one of the terminal states with the
 * result the client also received. A client that keeps a task ID can therefore check later what became of its request
 * - which matters as soon as an operation outlives the call that started it (Phase 4's Python execution).
 *
 * A record is created by the registry of its session and not directly. It holds what the request was, what it was
 * allowed to do, how far it came and what it answered; it never owns the objects it operated on. Whenever it changes
 * state, reports progress or records an activity, it writes the corresponding entry into the session's event log, so
 * the log is the history of what happened rather than a summary written afterwards.
 *
 * A pointer to a record is valid until the registry drops it from its bounded history; a caller that keeps one longer
 * looks the task up by ID again.
 */
class OVITO_CORE_EXPORT AutomationTaskRecord
{
public:

    AutomationTaskRecord(const AutomationTaskRecord&) = delete;
    AutomationTaskRecord& operator=(const AutomationTaskRecord&) = delete;

    /// The ID of this task, `task:t1`. Numbers are allocated once per session and never reused.
    const QString& id() const { return _id; }

    /// The client that asked for it, and where that client's activity comes from.
    const QString& clientId() const { return _clientId; }
    AutomationContract::ActivityOrigin origin() const { return _origin; }

    /// What was asked for.
    const QString& operationId() const { return _operationId; }
    const QString& requestId() const { return _requestId; }
    quint64 baseRevision() const { return _baseRevision; }

    /// The capabilities the operation required, and what the client held when the task started.
    const QVector<AutomationContract::Capability>& requiredCapabilities() const { return _requiredCapabilities; }
    const QStringList& grantedCapabilities() const { return _grantedCapabilities; }

    AutomationContract::TaskState state() const { return _state; }
    bool isRunning() const { return _state == AutomationContract::TaskState::Pending || _state == AutomationContract::TaskState::Running; }
    bool isFinished() const { return AutomationContract::isTerminalTaskState(_state); }

    /// The fraction of the work that is done, when the operation reported one.
    const std::optional<double>& progress() const { return _progress; }
    const QString& progressText() const { return _progressText; }

    QDateTime startedAt() const { return _startedAt; }
    QDateTime finishedAt() const { return _finishedAt; }

    /// Whether a client asked this task to stop. A cooperative operation checks this and answers Cancelled.
    bool isCancellationRequested() const { return _cancellationRequested; }

    /// The OVITO task this automation task is bound to, if any. Cancelling the automation task cancels it too.
    const std::shared_ptr<Task>& backingTask() const { return _backingTask; }

    /// The transaction boundary of this task, once it has one.
    const QString& transactionId() const { return _transactionId; }

    /// The answer of the operation: while the task runs it is the result the gateway prepared, afterwards the result
    /// the client received. It carries the revision, the warnings and, for a failure, the error code and message.
    const AutomationResult& result() const { return _result; }

    /// The revision the task left the session at, i.e. the revision of its result.
    quint64 revision() const { return _result.revision(); }

    /**
     * \brief Returns the task in wire form.
     * \param withResult Include the operation's payload. The list form leaves it out and the describe form includes
     *                   it, so that listing a session's history stays cheap.
     */
    QVariantMap toJson(bool withResult = false) const;

private:

    friend class AutomationTaskRegistry;

    /// The operation reports through these two helpers of the namespace below, which are the only callers.
    friend void this_automation_task::progress(double, QString);
    friend void this_automation_task::recordActivity(QString, QVariantMap);

    AutomationTaskRecord(QString id, QString clientId, AutomationContract::ActivityOrigin origin, QString operationId,
                         QString requestId, quint64 baseRevision, AutomationAuditTrail auditTrail);

    /// Moves the task from Pending to Running.
    void setRunning();

    /// Records the answer and the terminal state the answer implies.
    void finish(AutomationResult result);

    /// Reports progress; called through the ambient task of a running operation.
    void setProgress(double fraction, QString text);

    /// Records an activity of the running operation.
    void addActivity(QString summary, QVariantMap details);

    void requestCancellation();

    QString _id;
    QString _clientId;
    AutomationContract::ActivityOrigin _origin = AutomationContract::ActivityOrigin::User;
    QString _operationId;
    QString _requestId;
    quint64 _baseRevision = 0;

    QVector<AutomationContract::Capability> _requiredCapabilities;
    QStringList _grantedCapabilities;

    AutomationContract::TaskState _state = AutomationContract::TaskState::Pending;
    std::optional<double> _progress;
    QString _progressText;
    QDateTime _startedAt;
    QDateTime _finishedAt;
    bool _cancellationRequested = false;

    std::shared_ptr<Task> _backingTask;
    QString _transactionId;
    AutomationResult _result;
    AutomationAuditTrail _auditTrail;
};

/**
 * \brief The tasks of one session: allocation, lookup and the bounded history of finished ones.
 *
 * The registry is the session's task table, not a scheduler. It hands out IDs that are unique for the life of the
 * session, keeps every running task, and retains the newest finished ones up to a limit so that a client which comes
 * back later can still read what it (or someone else) started. Older finished tasks are dropped, and their IDs are not
 * reused: a client that asks for a dropped task is told that it is unknown, which is honest - the session cannot say
 * anything about a task it no longer remembers.
 *
 * The registry is not thread-safe; it belongs to the main thread, like the session and the gateway.
 */
class OVITO_CORE_EXPORT AutomationTaskRegistry
{
public:

    /// The number of finished tasks the registry retains when nothing else is asked for.
    static constexpr int defaultHistoryLimit = 64;

    AutomationTaskRegistry() = default;
    ~AutomationTaskRegistry();

    AutomationTaskRegistry(const AutomationTaskRegistry&) = delete;
    AutomationTaskRegistry& operator=(const AutomationTaskRegistry&) = delete;

    /**
     * \brief Creates a task for an operation that is about to run, and logs that it started.
     * \param requiredCapabilities The capabilities the operation's descriptor declares.
     * \param grantedCapabilities What the dispatching client held, for the record: enforcement happens per request,
     *                            and a later revocation does not rewrite what a finished task was allowed to do.
     * \return The new record, which lives in this registry until it is dropped from the history.
     */
    AutomationTaskRecord* begin(AutomationAuditTrail auditTrail, QString clientId, AutomationContract::ActivityOrigin origin,
                                QString operationId, QString requestId, quint64 baseRevision,
                                QVector<AutomationContract::Capability> requiredCapabilities, QStringList grantedCapabilities);

    /// The task with an ID, or null when the registry does not know it.
    AutomationTaskRecord* find(const QString& id) const;

    /**
     * \brief Whether the registry ever handed out the number of this ID.
     *
     * A task ID that names a task which was dropped from the bounded history reads differently from one that was never
     * issued: the first is a released identity (the client should re-query), the second is a malformed request. The
     * distinction mirrors AutomationObjectRegistry::wasAssigned().
     */
    bool wasDropped(const QString& id) const;

    /// Moves a task to Running. The gateway calls this immediately before it invokes the operation's handler.
    void start(AutomationTaskRecord& record);

    /// Binds a task to the transaction boundary it runs in, for the record and for the log.
    void attachTransaction(AutomationTaskRecord& record, QString transactionId);

    /// The retained tasks, newest first.
    QVector<AutomationTaskRecord*> tasks() const;

    /// The number of retained tasks.
    int size() const { return static_cast<int>(_records.size()); }

    /// Records the answer of a task and ends it. The terminal state follows the result: a success completes the task,
    /// a Cancelled error cancels it, any other failure fails it.
    void finish(AutomationTaskRecord& record, AutomationResult result);

    /**
     * \brief Asks a task to stop.
     * \return False when the registry does not know the task. A task that has already finished is not changed; the
     *         caller decides whether that is worth a warning (see the gateway's task.cancel operation).
     */
    bool requestCancel(const QString& id);

    /// Binds a task to an OVITO task, so that cancelling the automation task cancels the work behind it.
    void attachTask(AutomationTaskRecord& record, std::shared_ptr<Task> task);

    /// How many finished tasks the registry retains. Running tasks are never dropped.
    int historyLimit() const { return _historyLimit; }
    void setHistoryLimit(int historyLimit);

    /// Drops every finished task from the history. Running tasks stay.
    void clear();

private:

    /// Drops the oldest finished tasks so that the history stays bounded.
    void trimHistory();

    std::list<std::unique_ptr<AutomationTaskRecord>> _records;
    quint64 _nextNumber = 1;
    int _historyLimit = defaultHistoryLimit;
};

/**
 * \brief Makes one automation task the ambient task of the current thread for as long as it exists.
 *
 * A handler runs inside the dispatch of its operation and reports what it does through this_task-style helpers
 * (namespace this_automation_task below). The scope is what those helpers read, so an operation never needs a handle
 * of its own: like the rest of the OVITO core, the running work finds its context through the ambient task.
 */
class OVITO_CORE_EXPORT AutomationTaskScope
{
public:

    /// Installs a task; a null task is what a dispatch without a task record (a query) installs.
    explicit AutomationTaskScope(AutomationTaskRecord* task) noexcept;
    ~AutomationTaskScope();

    AutomationTaskScope(const AutomationTaskScope&) = delete;
    AutomationTaskScope& operator=(const AutomationTaskScope&) = delete;
    AutomationTaskScope(AutomationTaskScope&&) = delete;
    AutomationTaskScope& operator=(AutomationTaskScope&&) = delete;

private:

    /// The task that was ambient before this scope, restored when it ends. An operation may dispatch another one
    /// (a command that uses the gateway for part of its work), so a scope nests instead of insisting on being alone.
    AutomationTaskRecord* _previous = nullptr;
};

/**
 * \brief The automation task an operation runs in, in the way OVITO core exposes this_task.
 *
 * These helpers are for the implementation of an operation. A handler that reports progress, checks whether a client
 * asked it to stop, or records an activity of its own uses them; nothing else does. Outside the execution of a
 * command - in a query, in a frontend, or in a test - there is no ambient task and every call here does nothing,
 * which is why an operation must not depend on the channel being there to be correct.
 */
namespace this_automation_task {

/// The task of the operation currently running, or null.
OVITO_CORE_EXPORT AutomationTaskRecord* get() noexcept;

/// Reports how far the operation has come, as a fraction between 0 and 1, with an optional description.
OVITO_CORE_EXPORT void progress(double fraction, QString text = {});

/// Records a semantic activity of the running operation, for example "loaded a Python module".
OVITO_CORE_EXPORT void recordActivity(QString summary, QVariantMap details = {});

/// Reports whether a client asked the running task to stop.
OVITO_CORE_EXPORT bool isCancellationRequested() noexcept;

/**
 * \brief Throws OperationCanceled when the running task was asked to stop.
 *
 * A cooperative operation calls this between its steps; the gateway turns the exception into a Cancelled result and
 * the task ends in the Cancelled state. This is the same exception the rest of OVITO raises for cancelled work, so a
 * long step that runs through the task system cancels consistently with the rest of the application.
 */
OVITO_CORE_EXPORT void throwIfCancellationRequested();

}   // namespace this_automation_task

}   // End of namespace
