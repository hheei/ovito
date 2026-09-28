// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito::detail {

/**
 * \brief The three terminal channels a finished task can be in.
 *
 * These mirror the value / error / stopped completion channels described in ARCHITECTURE.md §1.
 */
enum class TaskOutcome {
    Completed,  ///< Finished successfully with a value (neither canceled nor failed).
    Canceled,   ///< Stopped before completion (the stopped channel).
    Failed      ///< Finished carrying an exception (the error channel).
};

/**
 * \brief Classifies a *finished* task into its terminal channel.
 *
 * This is the single place that encodes the channel-priority policy: **cancellation dominates a
 * stored exception**, matching Task::getExceptionIfFailed(). It uses only Task's public observers,
 * adds no state, and is therefore a free function rather than a Task member (it is a derived *view*
 * over the task's state, not part of the task's identity).
 *
 * It is cheap and noexcept: it does not materialize the Exception object. Callers switch on the
 * result and only fetch the exception (via Task::getExceptionIfFailed()) in the Failed branch,
 * avoiding the rethrow/recatch cost on paths that merely need to classify the outcome.
 */
[[nodiscard]] inline TaskOutcome decodeOutcome(const Task& task) noexcept
{
    OVITO_ASSERT(task.isFinished());
    if(task.isCanceled())
        return TaskOutcome::Canceled;   // Cancellation dominates a stored exception.
    if(task.exceptionStore())
        return TaskOutcome::Failed;
    return TaskOutcome::Completed;
}

}   // End of namespace
