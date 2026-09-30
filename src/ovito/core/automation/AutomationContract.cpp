// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationContract.h>

namespace Ovito {

/******************************************************************************
* Returns the contract version as "<major>.<minor>".
******************************************************************************/
QString AutomationContract::version()
{
    return QStringLiteral("%1.%2").arg(versionMajor).arg(versionMinor);
}

/******************************************************************************
* Returns the wire name of an operation kind.
******************************************************************************/
QString AutomationContract::kindName(OperationKind kind)
{
    switch(kind) {
        case OperationKind::Query: return QStringLiteral("query");
        case OperationKind::Command: return QStringLiteral("command");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Returns the wire name of a capability.
******************************************************************************/
QString AutomationContract::capabilityName(Capability capability)
{
    switch(capability) {
        case Capability::SessionRead: return QStringLiteral("session.read");
        case Capability::SceneRead: return QStringLiteral("scene.read");
        case Capability::SelectionRead: return QStringLiteral("selection.read");
        case Capability::FileRead: return QStringLiteral("file.read");
        case Capability::PipelineWrite: return QStringLiteral("pipeline.write");
        case Capability::SelectionWrite: return QStringLiteral("selection.write");
        case Capability::AnimationWrite: return QStringLiteral("animation.write");
        case Capability::PythonExecute: return QStringLiteral("python.execute");
        case Capability::FileWrite: return QStringLiteral("file.write");
        case Capability::NetworkAccess: return QStringLiteral("network.access");
        case Capability::ProcessExecute: return QStringLiteral("process.execute");
        case Capability::TaskControl: return QStringLiteral("task.control");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Looks a capability up by its wire name.
******************************************************************************/
std::optional<AutomationContract::Capability> AutomationContract::capabilityFromName(QStringView name)
{
    // The list is short and ordered by the enum, so a linear scan is both the simplest and the most predictable way
    // to keep the two directions in step.
    for(int i = 0; i <= static_cast<int>(Capability::TaskControl); ++i) {
        const Capability capability = static_cast<Capability>(i);
        if(capabilityName(capability) == name)
            return capability;
    }
    return std::nullopt;
}

/******************************************************************************
* Returns whether a capability only permits reading.
******************************************************************************/
bool AutomationContract::isReadCapability(Capability capability)
{
    switch(capability) {
        case Capability::SessionRead:
        case Capability::SceneRead:
        case Capability::SelectionRead:
        case Capability::FileRead:
            return true;
        case Capability::PipelineWrite:
        case Capability::SelectionWrite:
        case Capability::AnimationWrite:
        case Capability::PythonExecute:
        case Capability::FileWrite:
        case Capability::NetworkAccess:
        case Capability::ProcessExecute:
        case Capability::TaskControl:
            return false;
    }
    OVITO_ASSERT(false);
    return false;
}

/******************************************************************************
* Returns the wire name of an error code.
******************************************************************************/
QString AutomationContract::errorCodeName(ErrorCode code)
{
    switch(code) {
        case ErrorCode::InvalidRequest: return QStringLiteral("invalid_request");
        case ErrorCode::UnknownOperation: return QStringLiteral("unknown_operation");
        case ErrorCode::InvalidArgument: return QStringLiteral("invalid_argument");
        case ErrorCode::MissingCapability: return QStringLiteral("missing_capability");
        case ErrorCode::StaleRevision: return QStringLiteral("stale_revision");
        case ErrorCode::UnknownObject: return QStringLiteral("unknown_object");
        case ErrorCode::InvalidatedObject: return QStringLiteral("invalidated_object");
        case ErrorCode::NotSupported: return QStringLiteral("not_supported");
        case ErrorCode::Cancelled: return QStringLiteral("cancelled");
        case ErrorCode::InternalError: return QStringLiteral("internal_error");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Returns the wire name of a task state.
******************************************************************************/
QString AutomationContract::taskStateName(TaskState state)
{
    switch(state) {
        case TaskState::Pending: return QStringLiteral("pending");
        case TaskState::Running: return QStringLiteral("running");
        case TaskState::Completed: return QStringLiteral("completed");
        case TaskState::Failed: return QStringLiteral("failed");
        case TaskState::Cancelled: return QStringLiteral("cancelled");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Returns whether a task state is final.
******************************************************************************/
bool AutomationContract::isTerminalTaskState(TaskState state)
{
    switch(state) {
        case TaskState::Pending:
        case TaskState::Running:
            return false;
        case TaskState::Completed:
        case TaskState::Failed:
        case TaskState::Cancelled:
            return true;
    }
    OVITO_ASSERT(false);
    return true;
}

/******************************************************************************
* Returns the wire name of an activity origin.
******************************************************************************/
QString AutomationContract::originName(ActivityOrigin origin)
{
    switch(origin) {
        case ActivityOrigin::User: return QStringLiteral("user");
        case ActivityOrigin::Qml: return QStringLiteral("qml");
        case ActivityOrigin::Cli: return QStringLiteral("cli");
        case ActivityOrigin::Ai: return QStringLiteral("ai");
        case ActivityOrigin::Python: return QStringLiteral("python");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Looks an activity origin up by its wire name.
******************************************************************************/
std::optional<AutomationContract::ActivityOrigin> AutomationContract::originFromName(QStringView name)
{
    for(int i = 0; i <= static_cast<int>(ActivityOrigin::Python); ++i) {
        const ActivityOrigin origin = static_cast<ActivityOrigin>(i);
        if(originName(origin) == name)
            return origin;
    }
    return std::nullopt;
}

/******************************************************************************
* Returns the wire name of a transaction state.
******************************************************************************/
QString AutomationContract::transactionStateName(TransactionState state)
{
    switch(state) {
        case TransactionState::Open: return QStringLiteral("open");
        case TransactionState::Committed: return QStringLiteral("committed");
        case TransactionState::Aborted: return QStringLiteral("aborted");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Returns the wire name of an event kind.
******************************************************************************/
QString AutomationContract::eventKindName(EventKind kind)
{
    switch(kind) {
        case EventKind::SessionChanged: return QStringLiteral("session.changed");
        case EventKind::TaskStarted: return QStringLiteral("task.started");
        case EventKind::TaskProgress: return QStringLiteral("task.progress");
        case EventKind::TaskFinished: return QStringLiteral("task.finished");
        case EventKind::Activity: return QStringLiteral("activity");
    }
    OVITO_ASSERT(false);
    return {};
}

/******************************************************************************
* Looks an event kind up by its wire name.
******************************************************************************/
std::optional<AutomationContract::EventKind> AutomationContract::eventKindFromName(QStringView name)
{
    for(int i = 0; i <= static_cast<int>(EventKind::Activity); ++i) {
        const EventKind kind = static_cast<EventKind>(i);
        if(eventKindName(kind) == name)
            return kind;
    }
    return std::nullopt;
}

}   // End of namespace
