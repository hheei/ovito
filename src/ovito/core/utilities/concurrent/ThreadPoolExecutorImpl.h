// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include "ThreadPoolExecutor.h"

namespace Ovito {

template<typename Function>
inline void ThreadPoolExecutor::execute(Function&& f) const noexcept
{
    static_assert(std::is_invocable_v<Function>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");

    TaskManager& taskManager = Application::instance()->taskManager();

    // High-priority work (the interactive viewport-render subtree) runs at normal OS priority and
    // oversubscribes the pool so it starts promptly; regular work runs at low priority. The priority is taken
    // from the executor flag, or inherited from the task that is scheduling this work.
    const Task* currentTask = this_task::get();
    bool highPriority = _highPriority || (currentTask && currentTask->isHighPriorityTask());

    // Wrap the callable function in a Qt runner object.
    struct Runner : public QRunnable
    {
        std::decay_t<Function> f;
        bool highPriority;
        explicit Runner(Function&& f, bool highPriority) : f(std::forward<Function>(f)), highPriority(highPriority) {}
        virtual void run() final override {
#ifdef QT_BUILDING_UNDER_TSAN
            // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
            // This annotation establishes a happens-after relation with the corresponding __tsan_release() call when this runnable is submitted to the thread pool.
            ::__tsan_acquire(this);
#endif
            // For high-priority work, startWork() raised the pool's thread limit so this worker could start
            // immediately; restore the limit once we are done running (see TaskManager::startWork()).
            struct OversubscriptionGuard {
                bool active;
                ~OversubscriptionGuard() { if(active) Application::instance()->taskManager().endOversubscription(); }
            } oversubscriptionGuard{highPriority};

            // Run at the OS scheduling priority appropriate for this work item (see WorkerThreadPriorityScope).
            TaskManager::WorkerThreadPriorityScope priorityScope(highPriority);
            std::invoke(std::move(f));
        }
    };
    Runner* runner = new Runner(std::forward<Function>(f), highPriority);

#ifdef QT_BUILDING_UNDER_TSAN
    // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
    // This annotation establishes a happens-before relation with the corresponding __tsan_acquire() call in the worker function executed in the thread pool.
    ::__tsan_release(runner);
#endif

    // Submit runner to the thread pool (oversubscribing for high-priority work).
    taskManager.startWork(runner, highPriority);
}

}   // End of namespace