// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationTask.h>
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito {

namespace {

/// The task of the operation this thread is currently running, in the way this_task holds the ambient OVITO task.
thread_local AutomationTaskRecord* _currentTask = nullptr;

}   // End of anonymous namespace

/******************************************************************************
* Constructs a task record.
******************************************************************************/
AutomationTaskRecord::AutomationTaskRecord(QString id, QString clientId, AutomationContract::ActivityOrigin origin,
                                           QString operationId, QString requestId, quint64 baseRevision,
                                           AutomationAuditTrail auditTrail)
    : _id(std::move(id))
    , _clientId(std::move(clientId))
    , _origin(origin)
    , _operationId(std::move(operationId))
    , _requestId(std::move(requestId))
    , _baseRevision(baseRevision)
    , _auditTrail(auditTrail)
{
}

/******************************************************************************
* Moves the task from Pending to Running.
******************************************************************************/
void AutomationTaskRecord::setRunning()
{
    OVITO_ASSERT(_state == AutomationContract::TaskState::Pending);
    _state = AutomationContract::TaskState::Running;
    _startedAt = QDateTime::currentDateTimeUtc();
}

/******************************************************************************
* Records the answer of the task and ends it.
******************************************************************************/
void AutomationTaskRecord::finish(AutomationResult result)
{
    OVITO_ASSERT(isRunning());
    // The terminal state follows the answer, so a client never has to compare two things to know how a request ended:
    // a success completed the task, a cancelled request cancelled it, and anything else failed.
    if(result.isSuccess())
        _state = AutomationContract::TaskState::Completed;
    else if(result.errorCode() == AutomationContract::ErrorCode::Cancelled || _cancellationRequested)
        _state = AutomationContract::TaskState::Cancelled;
    else
        _state = AutomationContract::TaskState::Failed;
    if(!_progress && result.isSuccess())
        _progress = 1.0;
    _finishedAt = QDateTime::currentDateTimeUtc();
    _result = std::move(result);

    QString summary;
    if(_state == AutomationContract::TaskState::Completed)
        summary = QStringLiteral("Task '%1' of operation '%2' completed.").arg(_id, _operationId);
    else if(_state == AutomationContract::TaskState::Cancelled)
        summary = QStringLiteral("Task '%1' of operation '%2' was cancelled.").arg(_id, _operationId);
    else
        summary = QStringLiteral("Task '%1' of operation '%2' failed: %3").arg(_id, _operationId, _result.errorMessage());
    _auditTrail.record(AutomationContract::EventKind::TaskFinished, std::move(summary), _origin, _clientId, _id,
                       _transactionId, _operationId,
                       { { QStringLiteral("state"), AutomationContract::taskStateName(_state) },
                         { QStringLiteral("errorCode"), _result.isError() ? AutomationContract::errorCodeName(_result.errorCode()) : QString() } });
}

/******************************************************************************
* Reports how far the operation has come.
******************************************************************************/
void AutomationTaskRecord::setProgress(double fraction, QString text)
{
    OVITO_ASSERT(isRunning());
    // A progress report is a snapshot, not a state machine: a fraction below the previous one is accepted (an
    // operation may restart a step) and a report without a text keeps the previous one.
    _progress = qBound(0.0, fraction, 1.0);
    if(!text.isEmpty())
        _progressText = std::move(text);
    _auditTrail.record(AutomationContract::EventKind::TaskProgress, _progressText, _origin, _clientId, _id,
                       _transactionId, _operationId,
                       { { QStringLiteral("progress"), *_progress } });
}

/******************************************************************************
* Records an activity of the running operation.
******************************************************************************/
void AutomationTaskRecord::addActivity(QString summary, QVariantMap details)
{
    _auditTrail.record(AutomationContract::EventKind::Activity, std::move(summary), _origin, _clientId, _id, _transactionId,
                       _operationId, std::move(details));
}

/******************************************************************************
* Marks the task as asked to stop, and cancels the OVITO task behind it.
******************************************************************************/
void AutomationTaskRecord::requestCancellation()
{
    _cancellationRequested = true;
    // An operation that runs through the OVITO task system cancels the same way the rest of the application does, so
    // a bound task is cancelled here rather than only being told about it.
    if(_backingTask && !_backingTask->isFinished())
        _backingTask->cancel();
}

/******************************************************************************
* Returns the task in wire form.
******************************************************************************/
QVariantMap AutomationTaskRecord::toJson(bool withResult) const
{
    QVariantMap json;
    json.insert(QStringLiteral("id"), _id);
    json.insert(QStringLiteral("state"), AutomationContract::taskStateName(_state));
    json.insert(QStringLiteral("operationId"), _operationId);
    json.insert(QStringLiteral("clientId"), _clientId);
    json.insert(QStringLiteral("origin"), AutomationContract::originName(_origin));
    json.insert(QStringLiteral("baseRevision"), QVariant::fromValue<qulonglong>(_baseRevision));
    json.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(revision()));
    json.insert(QStringLiteral("cancellationRequested"), _cancellationRequested);
    if(!_requestId.isEmpty())
        json.insert(QStringLiteral("requestId"), _requestId);
    if(_progress)
        json.insert(QStringLiteral("progress"), *_progress);
    if(!_progressText.isEmpty())
        json.insert(QStringLiteral("progressText"), _progressText);
    if(_startedAt.isValid())
        json.insert(QStringLiteral("startedAt"), _startedAt.toString(Qt::ISODateWithMs));
    if(_finishedAt.isValid())
        json.insert(QStringLiteral("finishedAt"), _finishedAt.toString(Qt::ISODateWithMs));
    if(!_transactionId.isEmpty())
        json.insert(QStringLiteral("transactionId"), _transactionId);
    if(!_grantedCapabilities.isEmpty())
        json.insert(QStringLiteral("grantedCapabilities"), _grantedCapabilities);
    if(!_requiredCapabilities.isEmpty()) {
        QVariantList required;
        for(AutomationContract::Capability capability : _requiredCapabilities)
            required.push_back(AutomationContract::capabilityName(capability));
        json.insert(QStringLiteral("requiredCapabilities"), required);
    }
    if(isFinished()) {
        json.insert(QStringLiteral("warnings"), _result.warnings());
        if(_result.isError()) {
            json.insert(QStringLiteral("errorCode"), AutomationContract::errorCodeName(_result.errorCode()));
            json.insert(QStringLiteral("errorMessage"), _result.errorMessage());
        }
        if(withResult)
            json.insert(QStringLiteral("result"), _result.toJson());
    }
    return json;
}

/******************************************************************************
* Creates a task for an operation that is about to run.
******************************************************************************/
AutomationTaskRegistry::~AutomationTaskRegistry() = default;

AutomationTaskRecord* AutomationTaskRegistry::begin(AutomationAuditTrail auditTrail, QString clientId,
                                                    AutomationContract::ActivityOrigin origin, QString operationId,
                                                    QString requestId, quint64 baseRevision,
                                                    QVector<AutomationContract::Capability> requiredCapabilities,
                                                    QStringList grantedCapabilities)
{
    // The number is allocated before anything can fail, so that a task ID is a fact of the session rather than a
    // consequence of the operation's success.
    const QString id = QStringLiteral("task:t%1").arg(_nextNumber++);
    auto record = std::unique_ptr<AutomationTaskRecord>(new AutomationTaskRecord(
        id, std::move(clientId), origin, std::move(operationId), std::move(requestId), baseRevision, auditTrail));
    record->_requiredCapabilities = std::move(requiredCapabilities);
    record->_grantedCapabilities = std::move(grantedCapabilities);
    AutomationTaskRecord* task = record.get();
    _records.push_back(std::move(record));

    QVariantList requiredCapabilityNames;
    for(AutomationContract::Capability capability : task->_requiredCapabilities)
        requiredCapabilityNames.push_back(AutomationContract::capabilityName(capability));
    auditTrail.record(AutomationContract::EventKind::TaskStarted,
                      QStringLiteral("Task '%1' for operation '%2' started.").arg(task->id(), task->operationId()),
                      origin, task->clientId(), task->id(), {}, task->operationId(),
                      { { QStringLiteral("requiredCapabilities"), requiredCapabilityNames } });
    return task;
}

/******************************************************************************
* Looks a task up by ID.
******************************************************************************/
AutomationTaskRecord* AutomationTaskRegistry::find(const QString& id) const
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
bool AutomationTaskRegistry::wasDropped(const QString& id) const
{
    static const QString prefix = QStringLiteral("task:t");
    if(!id.startsWith(prefix))
        return false;
    bool ok = false;
    const quint64 number = id.mid(prefix.size()).toULongLong(&ok);
    return ok && number > 0 && number < _nextNumber;
}

/******************************************************************************
* Moves a task to Running.
******************************************************************************/
void AutomationTaskRegistry::start(AutomationTaskRecord& record)
{
    record.setRunning();
}

/******************************************************************************
* Binds a task to its transaction boundary.
******************************************************************************/
void AutomationTaskRegistry::attachTransaction(AutomationTaskRecord& record, QString transactionId)
{
    record._transactionId = std::move(transactionId);
}

/******************************************************************************
* The retained tasks, newest first.
******************************************************************************/
QVector<AutomationTaskRecord*> AutomationTaskRegistry::tasks() const
{
    QVector<AutomationTaskRecord*> result;
    result.reserve(static_cast<int>(_records.size()));
    for(auto it = _records.rbegin(); it != _records.rend(); ++it)
        result.push_back(it->get());
    return result;
}

/******************************************************************************
* Records the answer of a task and ends it.
******************************************************************************/
void AutomationTaskRegistry::finish(AutomationTaskRecord& record, AutomationResult result)
{
    record.finish(std::move(result));
    trimHistory();
}

/******************************************************************************
* Asks a task to stop.
******************************************************************************/
bool AutomationTaskRegistry::requestCancel(const QString& id)
{
    AutomationTaskRecord* task = find(id);
    if(!task)
        return false;
    if(task->isFinished())
        return true;
    task->requestCancellation();
    return true;
}

/******************************************************************************
* Binds a task to an OVITO task.
******************************************************************************/
void AutomationTaskRegistry::attachTask(AutomationTaskRecord& record, std::shared_ptr<Task> task)
{
    record._backingTask = std::move(task);
    if(record._cancellationRequested && record._backingTask && !record._backingTask->isFinished())
        record._backingTask->cancel();
}

/******************************************************************************
* Sets how many finished tasks the registry retains.
******************************************************************************/
void AutomationTaskRegistry::setHistoryLimit(int historyLimit)
{
    _historyLimit = historyLimit;
    trimHistory();
}

/******************************************************************************
* Drops the oldest finished tasks.
******************************************************************************/
void AutomationTaskRegistry::trimHistory()
{
    if(_historyLimit < 0)
        return;
    int finished = 0;
    for(const auto& record : _records) {
        if(record->isFinished())
            ++finished;
    }
    // The oldest records are at the front, so walking forward drops the oldest finished tasks first and never a
    // running one - a task a client may still be watching must not disappear from under it.
    for(auto it = _records.begin(); it != _records.end() && finished > _historyLimit;) {
        if((*it)->isFinished()) {
            it = _records.erase(it);
            --finished;
        }
        else {
            ++it;
        }
    }
}

/******************************************************************************
* Drops every finished task.
******************************************************************************/
void AutomationTaskRegistry::clear()
{
    for(auto it = _records.begin(); it != _records.end();) {
        if((*it)->isFinished())
            it = _records.erase(it);
        else
            ++it;
    }
}

/******************************************************************************
* Installs the ambient task of a running operation.
******************************************************************************/
AutomationTaskScope::AutomationTaskScope(AutomationTaskRecord* task) noexcept : _previous(std::exchange(_currentTask, task)) {}

AutomationTaskScope::~AutomationTaskScope()
{
    _currentTask = _previous;
}

/******************************************************************************
* The helpers an operation reports through.
******************************************************************************/
AutomationTaskRecord* this_automation_task::get() noexcept
{
    return _currentTask;
}

void this_automation_task::progress(double fraction, QString text)
{
    if(AutomationTaskRecord* task = get())
        task->setProgress(fraction, std::move(text));
}

void this_automation_task::recordActivity(QString summary, QVariantMap details)
{
    if(AutomationTaskRecord* task = get())
        task->addActivity(std::move(summary), std::move(details));
}

bool this_automation_task::isCancellationRequested() noexcept
{
    AutomationTaskRecord* task = get();
    return task && task->isCancellationRequested();
}

void this_automation_task::throwIfCancellationRequested()
{
    if(isCancellationRequested())
        throw OperationCanceled();
}

}   // End of namespace
