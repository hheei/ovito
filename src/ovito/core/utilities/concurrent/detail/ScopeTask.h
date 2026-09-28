// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "../Task.h"
#include "TaskCallback.h"

namespace Ovito::detail {

/**
 * \brief The shared state of a structured-concurrency scope (the owned/nursery tier).
 *
 * A ScopeTask is a Task that, when created as the child of an enclosing task, establishes a
 * *bidirectional* cancellation link with its parent:
 *  - parent canceled  ⇒  this scope task is canceled (downward stop propagation), and
 *  - this scope task canceled  ⇒  the parent is canceled (upward propagation).
 *
 * It is the common building block for both MainThreadOperation and TaskScope. It is not tied to the
 * main thread; the main-thread restriction lives in MainThreadOperation, not here.
 */
class ScopeTask : public Task, public detail::TaskCallback<ScopeTask>
{
public:

    /// Constructor.
    /// \param ui          The user interface this scope is associated with (may be null).
    /// \param parentTask  The enclosing task to link with, or null for an isolated scope.
    /// \param isInteractive  Whether the scope performs interactive (user-initiated) actions.
    ScopeTask(std::shared_ptr<UserInterface> ui, Task* parentTask, bool isInteractive) noexcept : Task(isInteractive ? Task::IsInteractive : Task::NoState) {
        setUserInterface(std::move(ui));
        if(parentTask) {
            // Sanity check: The parent cannot be in the finished state yet when the child task is being created.
            OVITO_ASSERT(!parentTask->isFinished());

            // Inherit the priority status from the parent task.
            if(parentTask->isHighPriorityTask())
                this->setHighPriorityTask();

            // When this sub-task gets canceled, we cancel the parent task too (upward propagation).
            this->registerContinuation([this]() noexcept {
                if(isCanceled() && callbackTask() && !callbackTask()->isCanceled()) {
                    callbackTask()->cancel();
                }
            });

            // Register a callback function to get notified when the parent task gets canceled (downward propagation).
            registerCallback(parentTask, true);
        }
    }

    /// Callback function, which is invoked whenever the state of the parent task changes.
    void taskStateChangedCallback(int state, MutexLock& lock) noexcept {
        if(state & Canceled)
            this->cancel();
    }
};

}   // End of namespace
