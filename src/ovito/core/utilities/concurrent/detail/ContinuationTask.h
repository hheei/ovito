// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "TaskWithStorage.h"
#include "TaskAwaiter.h"
#include "../InlineExecutor.h"
#include "../Promise.h"

namespace Ovito::detail {

/**
 * \brief The type of task that is returned by the Future::then() method.
 */
template<typename R, class TaskBase = Task>
class ContinuationTask : public TaskWithStorage<R, TaskBase>, public TaskAwaiter
{
public:

    /// Delegating constructor.
    explicit ContinuationTask(Task::State initialState = Task::NoState) : ContinuationTask(initialState, std::nullopt) {}

    /// Constructor initializing the results storage.
    template<typename InitialValue>
    explicit ContinuationTask(Task::State initialState, InitialValue&& initialResult) :
            TaskWithStorage<R, TaskBase>(initialState, std::forward<InitialValue>(initialResult)),
            TaskAwaiter(static_cast<Task&>(*this))
    {
        OVITO_ASSERT(!(initialState & (Task::Canceled | Task::Finished)));
    }

    /// Sets the result of this task upon completion of the preceding task.
    template<typename Function, typename FutureType>
    void fulfillWith(PromiseBase&& promise, Function&& f, FutureType&& future) noexcept {
        OVITO_ASSERT(promise.task().get() == this);
        OVITO_ASSERT(future.isFinished());

        // Skip the continuation if the task has been canceled.
        if(this->isCanceled())
            return;

        try {
            // Execute the continuation function in the scope of this task object.
            Task::Scope taskScope(this);

            // Inspect return value type of the continuation function.
            if constexpr(!detail::returns_future_v<Function, FutureType>) {
                // Continuation function returns a result value or void.
                if constexpr(!detail::returns_void_v<Function, FutureType>) {
                    // Function returns non-void results.
                    if constexpr(!std::is_invocable_v<Function, FutureType>)
                        if constexpr(!std::is_void_v<typename FutureType::result_type>) {
                            if constexpr(is_shared_future_v<FutureType>)
                                this->setResult(std::invoke(std::forward<Function>(f), future.task()->template getResult<typename FutureType::result_type>()));
                            else
                                this->setResult(std::invoke(std::forward<Function>(f), future.task()->template takeResult<typename FutureType::result_type>()));
                        }
                        else {
                            this->setResult(std::invoke(std::forward<Function>(f)));
                        }
                    else
                        this->setResult(std::invoke(std::forward<Function>(f), std::forward<FutureType>(future)));
                }
                else {
                    // Function returns void.
                    if constexpr(!std::is_invocable_v<Function, FutureType>) {
                        if constexpr(!std::is_void_v<typename FutureType::result_type>) {
                            if constexpr(is_shared_future_v<FutureType>)
                                std::invoke(std::forward<Function>(f), future.task()->template getResult<typename FutureType::result_type>());
                            else
                                std::invoke(std::forward<Function>(f), future.task()->template takeResult<typename FutureType::result_type>());
                        }
                        else {
                            std::invoke(std::forward<Function>(f));
                        }
                    }
                    else {
                        std::invoke(std::forward<Function>(f), std::forward<FutureType>(future));
                    }
                }
                this->setFinished();
            }
            else {
                // The continuation function returns a new future, whose result will be used to fulfill this task.
                std::decay_t<callable_result_t<Function, FutureType>> nextFuture;
                // Call the continuation function with the results of the finished task or the finished future itself.
                if constexpr(!std::is_invocable_v<Function, FutureType>) {
                    if constexpr(!std::is_void_v<typename FutureType::result_type>) {
                        nextFuture = std::invoke(std::forward<Function>(f), std::forward<FutureType>(future).result());
                    }
                    else {
                        std::forward<FutureType>(future).waitForFinished();
                        nextFuture = std::invoke(std::forward<Function>(f));
                    }
                }
                else {
                    nextFuture = std::invoke(std::forward<Function>(f), std::forward<FutureType>(future));
                }
                handleUnwrappedFuture(std::move(promise), std::move(nextFuture));
            }
        }
        catch(...) {
            this->captureExceptionAndFinish();
        }
    }

protected:

    /// Uses the results of the given future to fulfill this task.
    void handleUnwrappedFuture(PromiseBase promise, auto&& future) noexcept {
        // Schedule the continuation task to run once the new future completes.
        // We are passing the type of the future (Future or SharedFuture) to the callback routine via a template parameter,
        // because this information would otherwise get lost when we unpack the task dependency from the future.
        whenTaskFinishes<ContinuationTask, &ContinuationTask::finalResultsAvailable<is_shared_future_v<decltype(future)>>>(
            std::move(future),
            InlineExecutor{},
            std::move(promise));
    }

private:

    /// Callback function which gets invoked once the unwrapped future has completed.
    template<bool IsSharedFuture>
    void finalResultsAvailable(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
        // Lock access to this task.
        Task::MutexLock lock(*this);

        // There is a small chance that the continuation task was canceled in the meantime but hasn't let go of the awaited task yet
        // (because finishing and running the registered continuation functions is not an atomic operation).
        // We need to check for this situation here and bail out if it happened.
        if(this->isFinished())
            return;

        // If the awaited task failed, inherit the error state.
        if(finishedTask->exceptionStore()) {
            this->exceptionLocked(finishedTask->exceptionStore());
        }
        else {
            // Adopt result value from the completed task.
            if constexpr(!std::is_void_v<R>) {
                if constexpr(IsSharedFuture)
                    this->setResult(finishedTask->template getResult<R>());
                else
                    this->setResult(finishedTask->template takeResult<R>());
            }
        }

        this->finishLocked(lock);
    }
};

} // End of namespace
