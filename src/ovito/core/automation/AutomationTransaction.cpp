// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationTransaction.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/******************************************************************************
* Constructs a transaction record.
******************************************************************************/
AutomationTransactionRecord::AutomationTransactionRecord(QString id, QString clientId,
                                                         AutomationContract::ActivityOrigin origin, QString label,
                                                         quint64 openRevision)
    : _id(std::move(id))
    , _clientId(std::move(clientId))
    , _origin(origin)
    , _label(std::move(label))
    , _openRevision(openRevision)
{
}

void AutomationTransactionRecord::addCommand(QString operationId)
{
    OVITO_ASSERT(isOpen());
    _commands.push_back(std::move(operationId));
}

void AutomationTransactionRecord::finish(AutomationContract::TransactionState state, quint64 closeRevision,
                                         int recordedOperations)
{
    OVITO_ASSERT(isOpen());
    _state = state;
    _closeRevision = closeRevision;
    _recordedOperations = recordedOperations;
}

/******************************************************************************
* Returns the record in wire form.
******************************************************************************/
QVariantMap AutomationTransactionRecord::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("id"), _id);
    json.insert(QStringLiteral("state"), AutomationContract::transactionStateName(_state));
    json.insert(QStringLiteral("label"), _label);
    json.insert(QStringLiteral("clientId"), _clientId);
    json.insert(QStringLiteral("origin"), AutomationContract::originName(_origin));
    json.insert(QStringLiteral("commands"), _commands);
    json.insert(QStringLiteral("undoable"), _undoable);
    json.insert(QStringLiteral("recordedOperations"), _recordedOperations);
    json.insert(QStringLiteral("openRevision"), QVariant::fromValue<qulonglong>(_openRevision));
    json.insert(QStringLiteral("closeRevision"), QVariant::fromValue<qulonglong>(_closeRevision));
    return json;
}

/******************************************************************************
* Constructs the transaction registry of a session.
******************************************************************************/
AutomationTransactionRegistry::~AutomationTransactionRegistry() = default;

/******************************************************************************
* Creates a boundary and logs that it opened.
******************************************************************************/
AutomationTransactionRecord* AutomationTransactionRegistry::begin(AutomationAuditTrail auditTrail, QString clientId,
                                                                 AutomationContract::ActivityOrigin origin, QString label,
                                                                 quint64 openRevision)
{
    // One boundary at a time: see the class comment. The caller (the gateway or the session) checks this before it
    // creates one, so arriving here with an open boundary is a programming error rather than a client-visible state.
    OVITO_ASSERT(!openTransaction());
    const QString id = QStringLiteral("transaction:x%1").arg(_nextNumber++);
    auto record = std::unique_ptr<AutomationTransactionRecord>(
        new AutomationTransactionRecord(id, std::move(clientId), origin, std::move(label), openRevision));
    AutomationTransactionRecord* transaction = record.get();
    _records.push_back(std::move(record));

    auditTrail.record(AutomationContract::EventKind::Activity,
                      QStringLiteral("Transaction '%1' ('%2') opened.").arg(transaction->id(), transaction->label()),
                      origin, transaction->clientId(), {}, transaction->id());
    return transaction;
}

/******************************************************************************
* Looks a transaction up by ID.
******************************************************************************/
AutomationTransactionRecord* AutomationTransactionRegistry::find(const QString& id) const
{
    for(const auto& record : _records) {
        if(record->id() == id)
            return record.get();
    }
    return nullptr;
}

/******************************************************************************
* Whether the registry ever handed out this ID's number.
******************************************************************************/
bool AutomationTransactionRegistry::wasDropped(const QString& id) const
{
    static const QString prefix = QStringLiteral("transaction:x");
    if(!id.startsWith(prefix))
        return false;
    bool ok = false;
    const quint64 number = id.mid(prefix.size()).toULongLong(&ok);
    return ok && number > 0 && number < _nextNumber;
}

/******************************************************************************
* The retained transactions, newest first.
******************************************************************************/
QVector<AutomationTransactionRecord*> AutomationTransactionRegistry::transactions() const
{
    QVector<AutomationTransactionRecord*> result;
    result.reserve(static_cast<int>(_records.size()));
    for(auto it = _records.rbegin(); it != _records.rend(); ++it)
        result.push_back(it->get());
    return result;
}

/******************************************************************************
* The boundary that is currently open.
******************************************************************************/
AutomationTransactionRecord* AutomationTransactionRegistry::openTransaction() const
{
    for(const auto& record : _records) {
        if(record->isOpen())
            return record.get();
    }
    return nullptr;
}

/******************************************************************************
* Closes a record and logs the outcome.
******************************************************************************/
void AutomationTransactionRegistry::finish(AutomationTransactionRecord& record, AutomationContract::TransactionState state,
                                          quint64 closeRevision, int recordedOperations, AutomationAuditTrail auditTrail)
{
    record.finish(state, closeRevision, recordedOperations);
    const QString verb = state == AutomationContract::TransactionState::Committed ? QStringLiteral("committed")
                                                                                  : QStringLiteral("aborted");
    auditTrail.record(AutomationContract::EventKind::Activity,
                      QStringLiteral("Transaction '%1' ('%2') %3 after %4 command(s).")
                          .arg(record.id(), record.label(), verb)
                          .arg(record.commandCount()),
                      record.origin(), record.clientId(), {}, record.id(), {},
                      { { QStringLiteral("state"), AutomationContract::transactionStateName(state) },
                        { QStringLiteral("commands"), record.commands() } });
    trimHistory();
}

/******************************************************************************
* Sets how many closed transactions the registry retains.
******************************************************************************/
void AutomationTransactionRegistry::setHistoryLimit(int historyLimit)
{
    _historyLimit = historyLimit;
    trimHistory();
}

/******************************************************************************
* Drops the oldest closed transactions.
******************************************************************************/
void AutomationTransactionRegistry::trimHistory()
{
    if(_historyLimit < 0)
        return;
    int closed = 0;
    for(const auto& record : _records) {
        if(!record->isOpen())
            ++closed;
    }
    for(auto it = _records.begin(); it != _records.end() && closed > _historyLimit;) {
        if(!(*it)->isOpen()) {
            it = _records.erase(it);
            --closed;
        }
        else {
            ++it;
        }
    }
}

/******************************************************************************
* Drops every closed transaction.
******************************************************************************/
void AutomationTransactionRegistry::clear()
{
    for(auto it = _records.begin(); it != _records.end();) {
        if(!(*it)->isOpen())
            it = _records.erase(it);
        else
            ++it;
    }
}

/******************************************************************************
* Destroys a boundary that was left open.
******************************************************************************/
AutomationTransaction::~AutomationTransaction()
{
    // A boundary that is still open when it goes away must not leave half-applied changes behind: core's
    // UndoableTransaction behaves the same way, and an operation that threw on its way out relies on it.
    if(isOpen())
        abort();
}

/******************************************************************************
* Opens the boundary.
******************************************************************************/
void AutomationTransaction::begin(AutomationTransactionRegistry& registry, AutomationAuditTrail auditTrail,
                                  UserInterface* userInterface, QString clientId,
                                  AutomationContract::ActivityOrigin origin, QString label)
{
    OVITO_ASSERT(!_record);
    _registry = &registry;
    _auditTrail = auditTrail;
    _record = registry.begin(auditTrail, std::move(clientId), origin, std::move(label), auditTrail.currentRevision());

    if(userInterface) {
        // From here on every property change a command makes is recorded into this compound operation, because the
        // recording scope makes it the current one - the same mechanism UserInterface::performTransaction() uses.
        _undoTransaction = std::make_unique<UndoableTransaction>(*userInterface, _record->label());
        _recordingScope = std::make_unique<UndoSuspender>(_undoTransaction->operation());
        _record->setUndoable(true);
    }
}

/******************************************************************************
* Records that a command is part of this boundary.
******************************************************************************/
void AutomationTransaction::joinCommand(const QString& operationId)
{
    OVITO_ASSERT(_record);
    _record->addCommand(operationId);
}

/******************************************************************************
* A position inside the boundary, for reverting one command's share.
******************************************************************************/
int AutomationTransaction::mark() const
{
    if(_undoTransaction && _undoTransaction->operation())
        return _undoTransaction->snapshot();
    return -1;
}

void AutomationTransaction::revertTo(int mark)
{
    if(_undoTransaction && mark >= 0)
        _undoTransaction->revertTo(mark);
}

/******************************************************************************
* Whether the boundary is still open.
******************************************************************************/
bool AutomationTransaction::isOpen() const
{
    return _record && _record->isOpen();
}

/******************************************************************************
* Commits the recorded changes.
******************************************************************************/
void AutomationTransaction::commit()
{
    close(AutomationContract::TransactionState::Committed, false);
}

/******************************************************************************
* Rolls the recorded changes back.
******************************************************************************/
void AutomationTransaction::abort()
{
    close(AutomationContract::TransactionState::Aborted, true);
}

/******************************************************************************
* Closes the boundary and reports it.
******************************************************************************/
void AutomationTransaction::close(AutomationContract::TransactionState state, bool undoRecordedChanges)
{
    if(!_record)
        return;

    // Recording stops first: committing pushes the compound operation onto the undo stack, which the undo system only
    // allows while nothing is being recorded.
    _recordingScope.reset();

    int recorded = 0;
    if(_undoTransaction) {
        if(CompoundOperation* operation = _undoTransaction->operation())
            recorded = operation->count();
        if(undoRecordedChanges)
            _undoTransaction->cancel();
        else
            _undoTransaction->commit();
        _undoTransaction.reset();
    }

    AutomationTransactionRecord* record = _record;
    AutomationTransactionRegistry* registry = _registry;
    _record = nullptr;
    _registry = nullptr;
    registry->finish(*record, state, _auditTrail.currentRevision(), recorded, _auditTrail);
    _auditTrail = {};
}

}   // End of namespace
