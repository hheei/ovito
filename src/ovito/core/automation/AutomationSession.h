// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationEvent.h>
#include <ovito/core/automation/AutomationObjectRegistry.h>
#include <ovito/core/automation/AutomationTask.h>
#include <ovito/core/automation/AutomationTransaction.h>
#include <ovito/core/oo/OORef.h>

#include <memory>

namespace Ovito {

class DataSet;
class DataSetContainer;
class UserInterface;

/**
 * \brief The live session an automation client looks at: the data set, its revision, and the identities of its objects.
 *
 * This is the first half of the automation boundary of Phase 2.6. It answers "what does this session contain right
 * now" and nothing else: the operations that read or change it belong to AutomationGateway.
 *
 * A frontend attaches its data set to a session - the QtWidgets and Qt Quick workbenches pass what their
 * DataSetContainer holds - while a headless batch job owns the data set itself. Tests do the same. Because the session
 * is the only owner of the revision counter and of the object registry, every client of one session sees the same
 * identities and the same revision, no matter which frontend is running.
 *
 * The session is not thread-safe and belongs to the main thread.
 */
class OVITO_CORE_EXPORT AutomationSession : public QObject
{
public:

    explicit AutomationSession(QObject* parent = nullptr);
    ~AutomationSession() override;

    AutomationSession(const AutomationSession&) = delete;
    AutomationSession& operator=(const AutomationSession&) = delete;
    AutomationSession(AutomationSession&&) = delete;
    AutomationSession& operator=(AutomationSession&&) = delete;

    /// The current data set, or null while the session has none. The session holds a strong reference.
    DataSet* dataSet() const { return _dataSet; }

    /**
     * \brief Replaces the data set, which ends the validity of every object ID handed out so far.
     *
     * Call this when the session opens another data set (loading a session file) and with a null argument when it
     * closes one. The object registry forgets every object and the revision advances, so a client that still holds an
     * ID from before is told that its view is stale rather than receiving an object of the new data set.
     *
     * Setting the data set that is already current is not a change: it neither advances the revision nor ends the
     * validity of the IDs. A caller that means "something about the data set changed" advances the revision itself -
     * that is what the container connections below do.
     */
    void setDataSet(OORef<DataSet> dataSet);

    /// The revision of the session: an ordinal that advances whenever the session changes in a way a client can see.
    quint64 revision() const { return _revision; }

    /**
     * \brief Advances the revision and returns the new value.
     * \param cause What changed, for the event log: an operation ID, or the name of the container signal that fired.
     */
    quint64 bumpRevision(QString cause = {});

    /**
     * \brief Reports whether a request that was based on a revision still applies to the current state.
     *
     * A request without a base revision accepts any state - that is a client that does not care what it overwrites,
     * which a query may do. A request that names one is rejected once the session has moved on, which is what stops a
     * plan from being applied to a scene whose meaning changed underneath it.
     */
    bool acceptsRevision(std::optional<quint64> baseRevision) const { return !baseRevision || *baseRevision == _revision; }

    /// The object identities of this session.
    AutomationObjectRegistry& objects() { return _objects; }
    const AutomationObjectRegistry& objects() const { return _objects; }

    /// The tasks of this session: what was asked for, how far it came, and how it ended.
    AutomationTaskRegistry& tasks() { return _tasks; }
    const AutomationTaskRegistry& tasks() const { return _tasks; }

    /// The transaction boundaries of this session: which changes belong together as one undo step.
    AutomationTransactionRegistry& transactions() { return _transactions; }
    const AutomationTransactionRegistry& transactions() const { return _transactions; }

    /// The observable history of this session: session changes, task lifecycles and semantic activity.
    AutomationEventLog& events() { return _events; }
    const AutomationEventLog& events() const { return _events; }

    /// Where this session records what it did; see AutomationAuditTrail.
    AutomationAuditTrail auditTrail() { return AutomationAuditTrail{ &_events, &_revision }; }

    /**
     * \brief The user interface this session belongs to, if any.
     *
     * A session that knows its user interface can make a command's changes undoable as one step and can tell what the
     * current thread's user interface is; a session without one - a batch job, a contract test - still has identities,
     * revisions, tasks and transaction boundaries, only no undo. The interface is not owned: it outlives the session,
     * which attachToContainer() also relies on.
     */
    UserInterface* userInterface() const { return _userInterface; }
    void setUserInterface(UserInterface* userInterface) { _userInterface = userInterface; }

    /**
     * \brief Registers a client and returns the ID it will be known by, `client:c1`.
     *
     * Numbers are allocated once per session and never reused. Registering a client is an activity: it is what the
     * event log shows when a client connects, and it is where the origin of the client's activity comes from.
     */
    QString registerClient(AutomationContract::ActivityOrigin origin, QString name = {});

    /**
     * \brief Opens a transaction boundary that the following commands join.
     *
     * This is how several dispatched commands become one undo step, which is what a multi-command plan of a later phase
     * needs. The boundary belongs to the session, not to a client: every command dispatched while it is open adds
     * itself to it, whatever client sent it.
     *
     * The boundary object stays alive after it was committed or aborted, so that a caller holding it does not hold a
     * dangling pointer; its record belongs to the registry from then on and is looked up by ID (the object carries none
     * after it closed). It is replaced - and destroyed - when the next boundary is opened.
     * \return The open boundary, or null when one is already open.
     */
    AutomationTransaction* beginTransaction(QString label,
                                            AutomationContract::ActivityOrigin origin = AutomationContract::ActivityOrigin::User,
                                            QString clientId = {});

    /// The boundary that is currently open, or null.
    AutomationTransaction* openTransaction() const { return _openTransaction && _openTransaction->isOpen() ? _openTransaction.get() : nullptr; }

    /// Commits the open boundary, if there is one.
    void commitTransaction();

    /// Rolls the open boundary back, if there is one.
    void abortTransaction();

    /**
     * \brief Records a semantic activity of the session itself.
     *
     * For what the layer does outside the execution of an operation: a client connecting, a capability being granted,
     * a session file being opened. It writes one Activity event and returns its sequence number.
     */
    quint64 logActivity(QString summary, AutomationContract::ActivityOrigin origin = AutomationContract::ActivityOrigin::User,
                        QString clientId = {}, QVariantMap details = {});

    /**
     * \brief Attaches this session to a data set container, the way an interactive frontend does it.
     *
     * Follows the container's current data set and advances the revision whenever the container reports a change a
     * client can observe: another data set becomes current, the viewport layout, the active viewport, the selection or
     * the current frame changes. The connection lives as long as the session or the container; detaching happens by
     * destroying the session.
     *
     * A session can be attached to one container. The revision is coarse on purpose: it says "something you may have
     * looked at changed", not which operation did it. The semantic activity stream of a later deliverable refines that.
     */
    void attachToContainer(DataSetContainer& container);

private:

    /// Called by the container connections; advances the revision and, for a new data set, drops the identities.
    void sessionChanged(QString cause);

    OORef<DataSet> _dataSet;
    quint64 _revision = 1;
    AutomationObjectRegistry _objects;
    AutomationTaskRegistry _tasks;
    AutomationTransactionRegistry _transactions;
    AutomationEventLog _events;
    std::unique_ptr<AutomationTransaction> _openTransaction;
    UserInterface* _userInterface = nullptr;
    quint64 _nextClientNumber = 1;
};

}   // End of namespace
