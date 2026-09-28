// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include "detail/TaskWithStorage.h"
#include "detail/ContinuationTask.h"
#include "Future.h"
#include "LaunchTask.h"
#include "ThreadPoolExecutor.h"

namespace Ovito {

/******************************************************************************
* Turns any callable into an asynchronous task and returns a future for its result.
*
* The function is executed using the given executor, e.g., a ThreadPoolExecutor or DeferredObjectExecutor.
* Note: The function may never run if the future gets canceled before execution begins.
******************************************************************************/
template<typename Executor, typename Function>
[[nodiscard]] auto launchFunctionAsTask(Executor&& executor, Function&& function)
{
    // Infer the future to create. If the function returns a future, use it as is. Otherwise, wrap the result in a Future.
    using result_future_type = std::conditional_t<detail::is_future_v<std::invoke_result_t<Function>>,
                                                std::invoke_result_t<Function>,
                                                Future<std::invoke_result_t<Function>>>;

    // Determine the type of task object to use.
    using base_task_type = std::conditional_t<detail::is_future_v<std::invoke_result_t<Function>>,
                detail::ContinuationTask<typename result_future_type::result_type, Task>,
                TaskWithStorage<std::invoke_result_t<Function>, Task>>;

    class LaunchTask : public base_task_type
    {
    public:
        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = result_future_type;

        /// Constructor.
        explicit LaunchTask(Function&& function) :
            base_task_type(Task::NoState, std::nullopt),
            _function(std::forward<Function>(function)) {}

        /// Starts execution of the task.
        void operator()(Executor&& executor) {
            std::forward<Executor>(executor).execute([promise = PromiseBase(this->shared_from_this())]() mutable noexcept {
                static_cast<LaunchTask*>(promise.task().get())->invokeFunction(std::move(promise).takeTask());
            });
        }

        /// Runs the user function.
        void invokeFunction(Promise<typename result_future_type::result_type> promise) noexcept {
            if(promise.isCanceled())
                return;
            try {
                Task::Scope taskScope(this);
                if constexpr(!detail::is_future_v<std::invoke_result_t<Function>>) {
                    if constexpr(!std::is_void_v<std::invoke_result_t<Function>>)
                        promise.setResult(std::invoke(std::move(_function)));
                    else
                        std::invoke(std::move(_function));
                    promise.setFinished();
                }
                else {
                    auto future = std::invoke(std::move(_function)); // This may throw
                    this->handleUnwrappedFuture(std::move(promise), std::move(future)); // This cannot throw
                }
            }
            catch(const OperationCanceled&) {}
            catch(...) {
                OVITO_ASSERT(!promise.isFinished());
                promise.captureExceptionAndFinish();
            }
        }

    private:
        /// The function to be executed.
        std::decay_t<Function> _function;
    };

    return launchTask(
        std::make_shared<LaunchTask>(std::forward<Function>(function)),
        std::forward<Executor>(executor));
}

/******************************************************************************
* Launches the given function in a detached task without waiting for its results.
* Use this helper to run a function in a fire-and-forget manner.
*
* This is OVITO's spelling of std::execution::start_detached: the work is started and its completion
* (value/error) is not observed by anyone.
*
* The function is executed using the given executor, e.g., a ThreadPoolExecutor or DeferredObjectExecutor.
*
* The function may never run if the executor decides to stop executing tasks.
* The function must not return a value.
* Any exceptions thrown by the function are reported to the user if the function is executed in the main thread and a user interface is available. Otherwise, exceptions are just swallowed.
******************************************************************************/
template<typename Executor, typename Function>
void startDetached(Executor&& executor, Function&& function)
{
    static_assert(std::is_invocable_r_v<void, Function>, "The function must be callable with no arguments and should return no value.");

    auto task = std::make_shared<Task>();

    // Inherit the priority status, interactive flag, and user interface from the current task.
    task->inheritContextFromCurrentTask();

    executor.execute(
        [promise = PromiseBase(std::move(task)), function=std::forward<Function>(function)]() mutable noexcept {
            OVITO_ASSERT(!promise.isCanceled() && !promise.isFinished());
            try {
                Task::Scope taskScope(promise.task().get());
                std::invoke(std::move(function));
                promise.setFinished();
            }
            catch(const OperationCanceled&) {}
            catch(const Exception& ex) {
                OVITO_ASSERT(!promise.isFinished());
                if(this_task::isMainThread() && promise.task()->userInterface())
                    promise.task()->userInterface()->reportError(ex);
                promise.captureExceptionAndFinish();
            }
            catch(...) {
                OVITO_ASSERT(!promise.isFinished());
                promise.captureExceptionAndFinish();
            }
        });
}


/******************************************************************************
* Schedules a function for execution in a worker thread pool and returns a future for its result.
* The provided function is executed asynchronously and this function returns immediately.
*
* Note: The function may never run if the future gets canceled before execution begins.
******************************************************************************/
template<typename Function>
[[nodiscard]] inline auto asyncLaunch(Function&& f)
{
    // Determine if the calling task is a high-priority task, because it is responsible for real-time GUI updates.
    // If so, we also want to run the worker function with elevated priority to ensure that it gets executed as soon as possible.
    bool highPriority = false;
    if(const Task* parentTask = this_task::get())
        highPriority = parentTask->isHighPriorityTask();

    return launchFunctionAsTask(ThreadPoolExecutor(highPriority), std::forward<Function>(f));
}

/******************************************************************************
* Schedules a function for execution in a worker thread pool and waits for the result.
* This function blocks until the worker function has finished executing (even if the caller's task gets canceled).
* Thus, it's safe to use if the worker function is a lambda capturing some local variables by reference.
*
* This is OVITO's spelling of std::execution::sync_wait: launch work on the pool, then block the
* calling thread until it completes and hand back its result.
******************************************************************************/
template<typename Function>
inline auto syncWait(Function&& f)
{
    // Launch the function in a worker thread.
    auto future = asyncLaunch(std::forward<Function>(f));

    // Waits until the task has finished executing (but do not return early when canceled but not yet finished).
    future.waitForFinished(false);

    // Return the result of the function to the caller, if any.
    if constexpr(!std::is_void_v<std::invoke_result_t<Function>>) {
        return std::move(future).result();
    }
}

/******************************************************************************
* Variant of asyncLaunch() for asynchronous *member* tasks of an OvitoObject.
*
* It keeps the given object alive for the entire duration of the worker task by capturing a strong
* reference to it, and (if the function accepts it) invokes the function with a reference to the object.
* Use this instead of capturing a raw 'this' together with a manual "self = OORef<Self>(this)" keep-alive
* in the worker lambda: here the keep-alive is structural and cannot be forgotten. The remaining inputs
* the task touches (data objects, a FrameGraph, ...) must still be captured by owning references.
* See ARCHITECTURE.md §6 (the keep-alive rule).
******************************************************************************/
template<typename Self, typename Function>
    requires std::derived_from<std::remove_cvref_t<Self>, OvitoObject>
[[nodiscard]] inline auto asyncLaunch(Self* self, Function&& function)
{
    return asyncLaunch([guard = OORef<Self>(self), function = std::forward<Function>(function)]() mutable {
        if constexpr(std::is_invocable_v<Function&, decltype(*guard)>)
            return std::invoke(function, *guard);
        else
            return std::invoke(function);
    });
}

}   // End of namespace
