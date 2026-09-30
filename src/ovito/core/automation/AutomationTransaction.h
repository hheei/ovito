// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationEvent.h>

#include <list>
#include <memory>

namespace Ovito {

class UserInterface;
class UndoableTransaction;
class UndoSuspender;
class AutomationTransactionRegistry;

/**
 * \brief One transaction boundary of the automation layer: the commands that changed something together.
 *
 * A transaction is where automation meets the undo system. Every command is dispatched inside one, so what its handler
 * changes becomes exactly one step on the user's undo stack, and a command that reports a failure leaves nothing
 * behind: the boundary is rolled back instead of committed. Several commands can share one boundary - a caller opens a
 * transaction and the commands dispatched while it is open join it - which is what a later phase's plan execution
 * needs to make "these five changes" one undo step.
 *
 * A record is created by the registry of its session and lives in its bounded history afterwards, so a client can see
 * what its change was part of.
 */
class OVITO_CORE_EXPORT AutomationTransactionRecord
{
public:

    AutomationTransactionRecord(const AutomationTransactionRecord&) = delete;
    AutomationTransactionRecord& operator=(const AutomationTransactionRecord&) = delete;

    /// The ID of this transaction, `transaction:x1`. Numbers are allocated once per session and never reused.
    const QString& id() const { return _id; }

    /// The client that opened it, and where that client's activity comes from.
    const QString& clientId() const { return _clientId; }
    AutomationContract::ActivityOrigin origin() const { return _origin; }

    /// What the transaction's changes are called on the undo stack.
    const QString& label() const { return _label; }

    /// The operations that joined this boundary, in dispatch order.
    const QStringList& commands() const { return _commands; }
    int commandCount() const { return static_cast<int>(_commands.size()); }

    AutomationContract::TransactionState state() const { return _state; }
    bool isOpen() const { return _state == AutomationContract::TransactionState::Open; }

    /// The session revision the boundary was opened at, and the one it left behind when it closed.
    quint64 openRevision() const { return _openRevision; }
    quint64 closeRevision() const { return _closeRevision; }

    /**
     * \brief Whether the boundary's changes were recorded through a user interface.
     *
     * True when the session knows the user interface of the running frontend: its changes can then be undone as one
     * step where that interface has an undo stack, and `recordedOperations` counts the sub-operations the boundary
     * collected. A session without a user interface still has transaction boundaries - a batch job or a test has no
     * undo - but nothing to record them in, so a client that needs undo checks this flag instead of assuming it.
     */
    bool isUndoable() const { return _undoable; }

    /// How many sub-operations the boundary collected; 0 when nothing recorded them or the changes did not touch an
    /// undoable property.
    int recordedOperations() const { return _recordedOperations; }

    QVariantMap toJson() const;

private:

    friend class AutomationTransactionRegistry;
    friend class AutomationTransaction;

    AutomationTransactionRecord(QString id, QString clientId, AutomationContract::ActivityOrigin origin, QString label,
                                quint64 openRevision);

    void addCommand(QString operationId);
    void setUndoable(bool undoable) { _undoable = undoable; }
    void finish(AutomationContract::TransactionState state, quint64 closeRevision, int recordedOperations);

    QString _id;
    QString _clientId;
    AutomationContract::ActivityOrigin _origin = AutomationContract::ActivityOrigin::User;
    QString _label;
    QStringList _commands;
    AutomationContract::TransactionState _state = AutomationContract::TransactionState::Open;
    quint64 _openRevision = 0;
    quint64 _closeRevision = 0;
    bool _undoable = false;
    int _recordedOperations = 0;
};

/**
 * \brief The transaction boundaries of one session: allocation, lookup and a bounded history of closed ones.
 *
 * Only one boundary is open at a time. That is not a limitation of the registry but of the undo system underneath it:
 * the changes of one user action are one compound operation, and nesting them would make "undo" mean something the
 * user cannot predict. A second attempt to open a boundary is therefore a programming error, not a client-visible
 * condition.
 */
class OVITO_CORE_EXPORT AutomationTransactionRegistry
{
public:

    /// The number of closed transactions the registry retains when nothing else is asked for.
    static constexpr int defaultHistoryLimit = 64;

    AutomationTransactionRegistry() = default;
    ~AutomationTransactionRegistry();

    AutomationTransactionRegistry(const AutomationTransactionRegistry&) = delete;
    AutomationTransactionRegistry& operator=(const AutomationTransactionRegistry&) = delete;

    /// Creates a boundary, logs that it opened and returns its record.
    AutomationTransactionRecord* begin(AutomationAuditTrail auditTrail, QString clientId,
                                       AutomationContract::ActivityOrigin origin, QString label, quint64 openRevision);

    AutomationTransactionRecord* find(const QString& id) const;

    /// Whether the registry ever handed out the number of this ID; see AutomationTaskRegistry::wasDropped().
    bool wasDropped(const QString& id) const;

    /// The retained transactions, newest first.
    QVector<AutomationTransactionRecord*> transactions() const;

    /// The boundary that is currently open, or null.
    AutomationTransactionRecord* openTransaction() const;

    /// Closes a record, logs the outcome and trims the history of closed ones.
    void finish(AutomationTransactionRecord& record, AutomationContract::TransactionState state, quint64 closeRevision,
                int recordedOperations, AutomationAuditTrail auditTrail);

    int historyLimit() const { return _historyLimit; }
    void setHistoryLimit(int historyLimit);

    void clear();

private:

    void trimHistory();

    std::list<std::unique_ptr<AutomationTransactionRecord>> _records;
    quint64 _nextNumber = 1;
    int _historyLimit = defaultHistoryLimit;
};

/**
 * \brief A live transaction boundary: the recording scope while it is open, and the undo step when it closes.
 *
 * This is the object that makes the boundary real rather than a label. It opens a user interface's compound operation
 * (when the session knows one), so every change a command makes is recorded into it, and it commits that compound
 * operation to the undo stack on success or undoes the recorded changes on failure - the same mechanism a frontend
 * uses through UserInterface::performTransaction().
 *
 * The destructor aborts a boundary that is still open, exactly like core's UndoableTransaction: a boundary that was
 * left behind by an exception or a forgotten commit must not leave half-applied changes in the session.
 */
class OVITO_CORE_EXPORT AutomationTransaction
{
public:

    AutomationTransaction() = default;
    ~AutomationTransaction();

    AutomationTransaction(const AutomationTransaction&) = delete;
    AutomationTransaction& operator=(const AutomationTransaction&) = delete;
    AutomationTransaction(AutomationTransaction&&) = delete;
    AutomationTransaction& operator=(AutomationTransaction&&) = delete;

    /**
     * \brief Opens the boundary.
     * \param userInterface The interface whose compound operation records the changes, or null for a session without
     *                      one (a batch job, a test): the boundary still exists and is reported, it just records
     *                      nothing and has nothing to undo.
     */
    void begin(AutomationTransactionRegistry& registry, AutomationAuditTrail auditTrail, UserInterface* userInterface,
               QString clientId, AutomationContract::ActivityOrigin origin, QString label);

    /// Records that a command is part of this boundary.
    void joinCommand(const QString& operationId);

    /**
     * \brief A position inside the boundary, taken before a command runs.
     *
     * A command that fails inside a boundary someone else opened must not take the other commands' changes with it, so
     * the gateway marks the boundary before a command and reverts to the mark when that command fails.
     * \return The position, or -1 when the boundary is not recording (a session without a user interface), where
     *         individual commands cannot be rolled back apart from each other.
     */
    int mark() const;

    /// Undoes and forgets everything recorded after a mark, keeping the boundary open.
    void revertTo(int mark);

    /// Commits the recorded changes and closes the boundary.
    void commit();

    /// Rolls the recorded changes back and closes the boundary.
    void abort();

    bool isOpen() const;

    /**
     * \brief The record this boundary created, or null once it has closed.
     *
     * The record belongs to the session's registry, which keeps it in its bounded history; a caller that needs it after
     * the boundary closed keeps its ID from before and looks it up there.
     */
    const AutomationTransactionRecord* record() const { return _record; }
    AutomationTransactionRecord* record() { return _record; }

private:

    /// Closes the boundary and reports it, after the undo transaction has been settled.
    void close(AutomationContract::TransactionState state, bool undoRecordedChanges);

    AutomationTransactionRegistry* _registry = nullptr;
    AutomationTransactionRecord* _record = nullptr;
    AutomationAuditTrail _auditTrail;

    /// The recording scope through the user interface: alive exactly while the boundary collects changes.
    std::unique_ptr<UndoSuspender> _recordingScope;

    /// Core's transaction that owns the compound operation; absent when there is no user interface.
    std::unique_ptr<UndoableTransaction> _undoTransaction;
};

}   // End of namespace
