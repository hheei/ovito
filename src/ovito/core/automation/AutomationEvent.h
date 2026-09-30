// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationContract.h>

#include <QDateTime>
#include <QList>

namespace Ovito {

/**
 * \brief One entry of a session's event log: what happened, when, and to what it belonged.
 *
 * An event is the session's observable history. It exists for three consumers the later phases name: a client that
 * wants to know what changed since it last looked (instead of re-reading the whole session), an AI agent that needs to
 * attribute a change to the user or to itself, and the tests, which can state a rule about the order of events instead
 * of inferring it from the revision counter.
 *
 * An event names the revision it happened at, and it carries identifiers rather than objects: a task ID, a transaction
 * ID, an operation ID, a client ID. It deliberately carries no raw input (no mouse or key events), no file path and no
 * data-derived value unless a recorder put one into the details on purpose, which is what "semantic activity" means in
 * the design.
 */
class OVITO_CORE_EXPORT AutomationEvent
{
public:

    AutomationEvent() = default;
    AutomationEvent(AutomationContract::EventKind kind, quint64 sequence);

    // Fluent setup; a recorder builds the entry it logs in one expression.
    AutomationEvent& setRevision(quint64 revision);
    AutomationEvent& setOrigin(AutomationContract::ActivityOrigin origin);
    AutomationEvent& setClientId(QString clientId);
    AutomationEvent& setTaskId(QString taskId);
    AutomationEvent& setTransactionId(QString transactionId);
    AutomationEvent& setOperationId(QString operationId);
    /// A short human-readable description of what happened, for a log view. Never the only information: the kind, the
    /// IDs and the revision are what a client switches on.
    AutomationEvent& setSummary(QString summary);
    AutomationEvent& setDetails(QVariantMap details);

    AutomationContract::EventKind kind() const { return _kind; }

    /// The position of this event in the session's log. Sequence numbers start at 1, increase by one per event, and
    /// keep increasing when old events are dropped, so they order events across a bounded history.
    quint64 sequence() const { return _sequence; }

    quint64 revision() const { return _revision; }
    const QDateTime& timestamp() const { return _timestamp; }
    AutomationContract::ActivityOrigin origin() const { return _origin; }
    const QString& clientId() const { return _clientId; }
    const QString& taskId() const { return _taskId; }
    const QString& transactionId() const { return _transactionId; }
    const QString& operationId() const { return _operationId; }
    const QString& summary() const { return _summary; }
    const QVariantMap& details() const { return _details; }

    QVariantMap toJson() const;
    static AutomationEvent fromJson(const QVariantMap& json);

private:

    /// The log assigns the position and the timestamp of an entry when it accepts it; no other code may set them.
    friend class AutomationEventLog;

    AutomationContract::EventKind _kind = AutomationContract::EventKind::Activity;
    quint64 _sequence = 0;
    quint64 _revision = 0;
    QDateTime _timestamp;
    AutomationContract::ActivityOrigin _origin = AutomationContract::ActivityOrigin::User;
    QString _clientId;
    QString _taskId;
    QString _transactionId;
    QString _operationId;
    QString _summary;
    QVariantMap _details;
};

/**
 * \brief The bounded, in-memory event history of one session.
 *
 * The log is what makes "what happened lately" answerable without keeping an unbounded history: it retains the newest
 * `capacity()` events (256 by default) and drops the oldest, while the sequence numbers of the dropped ones stay
 * allocated. A client that asks for events after a sequence number it no longer reaches is told where the retained
 * history begins rather than silently receiving a gap-free answer to a question it cannot answer.
 *
 * The log belongs to the main thread, like the rest of the automation layer, and it is never written to disk: this
 * phase keeps its history in memory and leaves persistence to whoever builds a session store.
 */
class OVITO_CORE_EXPORT AutomationEventLog : public QObject
{
    Q_OBJECT

public:

    /// The number of events a log retains when nothing else is asked for.
    static constexpr int defaultCapacity = 256;

    explicit AutomationEventLog(QObject* parent = nullptr);
    ~AutomationEventLog() override;

    AutomationEventLog(const AutomationEventLog&) = delete;
    AutomationEventLog& operator=(const AutomationEventLog&) = delete;

    /**
     * \brief Appends an event, assigning its sequence number and its timestamp.
     * \param event What happened; its sequence number and timestamp are overwritten.
     * \return The sequence number the event was given.
     */
    quint64 append(AutomationEvent event);

    /// The sequence number of the oldest event still retained, or 0 while the log is empty.
    quint64 firstSequence() const;

    /// The sequence number of the newest event, or 0 while the log is empty.
    quint64 lastSequence() const;

    /// The number of events currently retained.
    int size() const { return _events.size(); }

    /**
     * \brief Returns the retained events newer than a sequence number, oldest first.
     * \param since Only events with a sequence number greater than this are returned; 0 means all retained events.
     * \param limit The maximum number of events to return, counting from the oldest one that matches; a value of 0 or
     *              less returns everything that matches.
     */
    QVector<AutomationEvent> events(quint64 since = 0, int limit = -1) const;

    /// The number of events the log retains. Reducing it drops the oldest events immediately.
    int capacity() const { return _capacity; }
    void setCapacity(int capacity);

    /// Empties the log. The sequence numbers of the dropped events stay allocated.
    void clear();

Q_SIGNALS:

    /// Emitted after an event was appended. A transport of a later phase turns this into a subscription; nothing in
    /// this phase pushes events anywhere.
    void eventAppended(quint64 sequence);

private:

    QList<AutomationEvent> _events;
    quint64 _nextSequence = 1;
    int _capacity = defaultCapacity;
};

/**
 * \brief Where the automation layer records what it did: the session's event log and the revision it belongs to.
 *
 * Every part of the layer that produces an event - a task that progresses, a transaction that opens, the gateway when
 * it grants a capability or runs an operation - holds one of these. It is a borrowed pair of pointers into the
 * session, not an owner: a record created by a registry has it, a session has it, and a test may construct one for a
 * log of its own. When the log is null (a registry used outside a session) the calls below do nothing, which is why
 * the code that records never has to check whether it runs inside a session.
 */
struct OVITO_CORE_EXPORT AutomationAuditTrail
{
    /// The log to append to, or null when nothing is recorded.
    AutomationEventLog* log = nullptr;

    /// The session's revision counter. It must outlive the trail, which the session guarantees: the counter is a
    /// member of the object that owns both the log and the registry.
    const quint64* revision = nullptr;

    bool isActive() const { return log != nullptr; }
    quint64 currentRevision() const { return revision ? *revision : 0; }

    /// Appends one event, filling in the revision of this trail. Every other field comes from the caller.
    quint64 record(AutomationContract::EventKind kind, QString summary = {},
                   AutomationContract::ActivityOrigin origin = AutomationContract::ActivityOrigin::User,
                   QString clientId = {}, QString taskId = {}, QString transactionId = {}, QString operationId = {},
                   QVariantMap details = {});
};

}   // End of namespace
