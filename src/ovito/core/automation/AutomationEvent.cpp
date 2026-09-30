// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationEvent.h>

namespace Ovito {

/******************************************************************************
* Constructs an event of a kind, at a position in the log.
******************************************************************************/
AutomationEvent::AutomationEvent(AutomationContract::EventKind kind, quint64 sequence)
    : _kind(kind), _sequence(sequence), _timestamp(QDateTime::currentDateTimeUtc())
{
}

AutomationEvent& AutomationEvent::setRevision(quint64 revision)
{
    _revision = revision;
    return *this;
}

AutomationEvent& AutomationEvent::setOrigin(AutomationContract::ActivityOrigin origin)
{
    _origin = origin;
    return *this;
}

AutomationEvent& AutomationEvent::setClientId(QString clientId)
{
    _clientId = std::move(clientId);
    return *this;
}

AutomationEvent& AutomationEvent::setTaskId(QString taskId)
{
    _taskId = std::move(taskId);
    return *this;
}

AutomationEvent& AutomationEvent::setTransactionId(QString transactionId)
{
    _transactionId = std::move(transactionId);
    return *this;
}

AutomationEvent& AutomationEvent::setOperationId(QString operationId)
{
    _operationId = std::move(operationId);
    return *this;
}

AutomationEvent& AutomationEvent::setSummary(QString summary)
{
    _summary = std::move(summary);
    return *this;
}

AutomationEvent& AutomationEvent::setDetails(QVariantMap details)
{
    _details = std::move(details);
    return *this;
}

/******************************************************************************
* Returns the event as a JSON-compatible map.
******************************************************************************/
QVariantMap AutomationEvent::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("kind"), AutomationContract::eventKindName(_kind));
    json.insert(QStringLiteral("sequence"), QVariant::fromValue<qulonglong>(_sequence));
    json.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_revision));
    // ISO 8601 in UTC, so that a client never has to guess a time zone; the log is a log, not a user interface.
    json.insert(QStringLiteral("timestamp"), _timestamp.toUTC().toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("origin"), AutomationContract::originName(_origin));
    if(!_clientId.isEmpty())
        json.insert(QStringLiteral("clientId"), _clientId);
    if(!_taskId.isEmpty())
        json.insert(QStringLiteral("taskId"), _taskId);
    if(!_transactionId.isEmpty())
        json.insert(QStringLiteral("transactionId"), _transactionId);
    if(!_operationId.isEmpty())
        json.insert(QStringLiteral("operationId"), _operationId);
    if(!_summary.isEmpty())
        json.insert(QStringLiteral("summary"), _summary);
    if(!_details.isEmpty())
        json.insert(QStringLiteral("details"), _details);
    return json;
}

/******************************************************************************
* Reads an event back from its JSON form.
******************************************************************************/
AutomationEvent AutomationEvent::fromJson(const QVariantMap& json)
{
    const std::optional<AutomationContract::EventKind> kind =
        AutomationContract::eventKindFromName(json.value(QStringLiteral("kind")).toString());
    AutomationEvent event(kind.value_or(AutomationContract::EventKind::Activity),
                          json.value(QStringLiteral("sequence")).toULongLong());
    event.setRevision(json.value(QStringLiteral("revision")).toULongLong());
    event.setOrigin(AutomationContract::originFromName(json.value(QStringLiteral("origin")).toString())
                        .value_or(AutomationContract::ActivityOrigin::User));
    event.setClientId(json.value(QStringLiteral("clientId")).toString());
    event.setTaskId(json.value(QStringLiteral("taskId")).toString());
    event.setTransactionId(json.value(QStringLiteral("transactionId")).toString());
    event.setOperationId(json.value(QStringLiteral("operationId")).toString());
    event.setSummary(json.value(QStringLiteral("summary")).toString());
    event.setDetails(json.value(QStringLiteral("details")).toMap());
    return event;
}

/******************************************************************************
* Constructs the event log of a session.
******************************************************************************/
AutomationEventLog::AutomationEventLog(QObject* parent) : QObject(parent) {}

AutomationEventLog::~AutomationEventLog() = default;

/******************************************************************************
* Appends an event.
******************************************************************************/
quint64 AutomationEventLog::append(AutomationEvent event)
{
    // The log owns the position and the time of an event: a recorder says what happened, not when it was recorded or
    // where it sits, which keeps the ordering correct even when a recorder builds the event before someone else logs it.
    const quint64 sequence = _nextSequence;
    event._sequence = sequence;
    event._timestamp = QDateTime::currentDateTimeUtc();
    _events.push_back(event);
    if(_capacity >= 0) {
        while(_events.size() > _capacity)
            _events.removeFirst();
    }
    ++_nextSequence;
    Q_EMIT eventAppended(sequence);
    return sequence;
}

/******************************************************************************
* The oldest sequence number the log still retains.
******************************************************************************/
quint64 AutomationEventLog::firstSequence() const
{
    return _events.empty() ? 0 : _events.front().sequence();
}

/******************************************************************************
* The newest sequence number in the log.
******************************************************************************/
quint64 AutomationEventLog::lastSequence() const
{
    return _events.empty() ? 0 : _events.back().sequence();
}

/******************************************************************************
* Returns the events newer than a sequence number.
******************************************************************************/
QVector<AutomationEvent> AutomationEventLog::events(quint64 since, int limit) const
{
    QVector<AutomationEvent> result;
    for(const AutomationEvent& event : _events) {
        if(event.sequence() <= since)
            continue;
        if(limit > 0 && result.size() >= limit)
            break;
        result.push_back(event);
    }
    return result;
}

/******************************************************************************
* Sets how many events the log retains.
******************************************************************************/
void AutomationEventLog::setCapacity(int capacity)
{
    _capacity = capacity;
    if(_capacity >= 0) {
        while(_events.size() > _capacity)
            _events.removeFirst();
    }
}

/******************************************************************************
* Empties the log.
******************************************************************************/
void AutomationEventLog::clear()
{
    _events.clear();
}

/******************************************************************************
* Appends one event to the log of an audit trail.
******************************************************************************/
quint64 AutomationAuditTrail::record(AutomationContract::EventKind kind, QString summary,
                                     AutomationContract::ActivityOrigin origin, QString clientId, QString taskId,
                                     QString transactionId, QString operationId, QVariantMap details)
{
    if(!log)
        return 0;
    AutomationEvent event(kind, 0);
    event.setRevision(currentRevision())
        .setOrigin(origin)
        .setClientId(std::move(clientId))
        .setTaskId(std::move(taskId))
        .setTransactionId(std::move(transactionId))
        .setOperationId(std::move(operationId))
        .setSummary(std::move(summary))
        .setDetails(std::move(details));
    return log->append(std::move(event));
}

}   // End of namespace
