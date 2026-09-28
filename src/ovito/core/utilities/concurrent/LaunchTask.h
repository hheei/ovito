// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito {

/**
 * Launches an asynchronous task by invoking the task's call operator.
 * It returns a future for the task's results.
 *
 * The task class must define a type named 'future_type',
 * which specifies the type of Future<T> or SharedFuture<T> to be returned
 * by the function.
*/
template<class TaskType, typename... Args>
[[nodiscard]] auto launchTask(std::shared_ptr<TaskType> task, Args&&... args)
{
    OVITO_ASSERT(task);

    // The task class must define a type named 'future_type', which specifies what kind of return value the task produces.
    using future_type = typename TaskType::future_type;

    // Inherit the priority status, interactive flag, and user interface from the current task.
    task->inheritContextFromCurrentTask();

    // Check at compile-time whether the task's call operator is defined.
    if constexpr(std::is_invocable_v<TaskType, Args...>) {

        // Make the task the active one.
        Task::Scope taskScope(task);

        // Launch the task by invoking its call operator.
        (*task)(std::forward<Args>(args)...);
    }
    else {
        // Make sure no args have been provided by the caller.
        static_assert(sizeof...(Args) == 0, "The task does not accept any arguments.");
    }

    // Return the future to the caller.
    return future_type::createFromTask(std::move(task));
}

}   // End of namespace
