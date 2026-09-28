////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include "Task.h"

#include <latch>

namespace Ovito {

/// Runs a parallel, cancellable loop by distributing the work over up to \a maxWorkers worker threads.
///
/// The \a setup function is invoked once in the calling thread, before any workers start, and receives
/// the actual number of workers that will be used. The \a kernel function is then invoked once per
/// worker as kernel(workerIndex, workerCount).
///
/// Note: The same \a kernel object is shared by all workers and is invoked concurrently from multiple
/// threads. The kernel must therefore be safe for concurrent invocation; it must not mutate shared
/// state without synchronization. Use the \a workerIndex argument to address per-worker storage (see
/// parallelForCollect()).
///
/// Precondition: A current task must be active in the calling thread (this_task::get() must be non-null),
/// either passed explicitly via \a task or established through a Task::Scope. The loop honors cancellation
/// of that task.
template<typename Setup, typename Kernel>
void parallelCancellable(size_t maxWorkers, Setup&& setup, Kernel&& kernel, Task* task = this_task::get())
{
    OVITO_ASSERT(task);
    if(task->isCanceled())
        throw OperationCanceled();

    if(maxWorkers == 0) {
        std::invoke(std::forward<Setup>(setup), 0);
        return;
    }

#ifndef OVITO_DISABLE_THREADING

    // Never block the UI thread. Instead, run the parallel loop in a worker thread and
    // handle UI events while waiting.
    if(this_task::isMainThread()) {
        syncWait([&]() {
            parallelCancellable(maxWorkers, std::forward<Setup>(setup), std::forward<Kernel>(kernel), task);
        });
        return;
    }

    // This limits the number of runner objects to be allocated.
    static constexpr size_t builtinMaxWorkers = 128;

    // Determine the number of workers to use.
    size_t workerCount = std::min({maxWorkers, builtinMaxWorkers, (size_t)std::latch::max()});

    // If the application is running in single-threaded mode, we don't use additional worker threads.
    // (Use the user-configured limit, not the pool's instantaneous max, which may be transiently raised by
    // high-priority oversubscription.)
    QThreadPool* pool = Application::instance()->taskManager().threadPool();
    if(Application::instance()->taskManager().maxThreadCount() == 1)
        workerCount = 1;

    // Run caller-provided setup function in the caller's thread.
    std::invoke(std::forward<Setup>(setup), workerCount);

    if(workerCount != 1) {

        struct Worker : public QRunnable
        {
            Kernel* kernel;
            std::latch* latch;
            Task* task;
            size_t workerIndex;
            size_t workerCount;
            std::exception_ptr exception;

            Worker(Kernel* kernel, std::latch* latch, Task* task, size_t workerIndex, size_t workerCount) noexcept
                : kernel(kernel), latch(latch), task(task), workerIndex(workerIndex), workerCount(workerCount) {}

            // Move constructor - needed for std::vector requirement 'MoveInsertable'.
            Worker(Worker&& other) noexcept
                : kernel(other.kernel), latch(other.latch), task(other.task), workerIndex(other.workerIndex), workerCount(other.workerCount), exception(std::move(other.exception)) {}

            virtual void run() final override {
#ifdef QT_BUILDING_UNDER_TSAN
                // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
                // This annotation establishes a happens-after relation with the corresponding __tsan_release() call when this runnable is submitted to the thread pool.
                ::__tsan_acquire(this);
#endif
                // Run at the OS scheduling priority appropriate for this task (see WorkerThreadPriorityScope).
                TaskManager::WorkerThreadPriorityScope priorityScope(task->isHighPriorityTask());
                try {
                    if(!task->isCanceled()) {
                        // Execute the work function in the scope of this task object.
                        Task::Scope taskScope(task);
                        (*kernel)(workerIndex, workerCount);
                    }
                }
                catch(OperationCanceled) {
                    OVITO_ASSERT(task->isCanceled());
                }
                catch(...) {
                    // Capture exceptions from worker threads to rethrow them from the master thread.
                    exception = std::current_exception();
                }
                latch->count_down();
            }
        };

        std::vector<Worker> workers;
        workers.reserve(workerCount);

        // Create workers.
        std::latch latch(workerCount);
        for(size_t t = 0; t < workerCount; t++) {
            Worker& worker = workers.emplace_back(
                &kernel,
                &latch,
                task,
                t,
                workerCount
            );
            worker.setAutoDelete(false);
        }

        // Submit workers to the thread pool.
        // Note: This needs to happen in a separate step, because a possible std::vector reallocation would lead to a race condition otherwise.
        for(Worker& worker : workers) {
#ifdef QT_BUILDING_UNDER_TSAN
            // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
            // This annotation establishes a happens-before relation with the corresponding __tsan_acquire() call in the worker function executed in the thread pool.
            ::__tsan_release(&worker);
#endif
            pool->start(&worker);
        }

        // Simultaneously execute as many workers as possible in the current thread.
        for(auto worker = workers.rbegin(); worker != workers.rend(); ++worker) {
            if(pool->tryTake(&*worker))
                worker->run();
        }

        // Wait for all workers to finish.
        latch.wait();

        // Rethrow any exceptions from workers.
        for(Worker& worker : workers) {
            if(worker.exception)
                std::rethrow_exception(worker.exception);
        }
    }
    else {
        // Single-threaded mode.
        kernel(0, 1);
    }

#else
    // No-threaded mode.
    kernel(0, 1);
#endif

    if(task->isCanceled())
        throw OperationCanceled();
}

template<typename Setup, typename Kernel>
void parallelForChunks(size_t loopCount, size_t minimumChunkSize, Setup&& setup, Kernel&& kernel)
{
    OVITO_ASSERT(minimumChunkSize != 0);
    size_t maxWorkerCount = (loopCount + minimumChunkSize - 1) / minimumChunkSize;

    parallelCancellable(maxWorkerCount,
        std::forward<Setup>(setup),
        [&](size_t workerIndex, size_t workerCount) {
            size_t chunkSize = (loopCount + workerCount - 1) / workerCount;
            OVITO_ASSERT(chunkSize != 0);
            size_t fromIndex = workerIndex * chunkSize;
            size_t toIndex = std::min(fromIndex + chunkSize, loopCount);
            if(toIndex > fromIndex)
                kernel(workerIndex, fromIndex, toIndex);
        }
    );
}

template<typename Kernel>
void parallelForChunks(size_t loopCount, size_t minimumChunkSize, Kernel&& kernel)
{
    parallelForChunks(loopCount, minimumChunkSize, [](size_t) noexcept {}, std::forward<Kernel>(kernel));
}

template<typename Setup, typename OuterKernel>
void parallelForInnerOuter(size_t loopCount, size_t minimumChunkSize, TaskProgress& progress, Setup&& setup, OuterKernel&& outerKernel)
{
    OVITO_ASSERT(minimumChunkSize != 0);

    Task* task = this_task::get();
    OVITO_ASSERT(task);

    if(loopCount != 0) {
        progress.setMaximum(loopCount);
    }

    // Each call to TaskProgress::incrementValueNoCancel() acquires a mutex that is shared across the
    // whole user interface and triggers a UI notification. To avoid serializing all worker threads on
    // that mutex, every worker accumulates its progress locally and flushes it to the shared progress
    // object only after a certain number of iterations. This bounds the total number of progress updates
    // (and UI notifications) to roughly a constant, independent of the number of workers or the chunk size.
    const size_t progressUpdateGranularity = std::max<size_t>(minimumChunkSize, loopCount / 1000);

    parallelForChunks(loopCount, minimumChunkSize, std::forward<Setup>(setup), [&](size_t workerIndex, size_t fromIndex, size_t toIndex) {
        // Progress accumulated by this worker that has not yet been reported to the shared progress object.
        size_t pendingProgress = 0;
        outerKernel([&](auto&& innerKernel) {
            for(size_t i = fromIndex; i != toIndex; ) {
                size_t end = std::min(i + minimumChunkSize, toIndex);
                size_t count = end - i;
                for(; i != end; ++i) {
                    if constexpr(std::is_invocable_v<decltype(innerKernel), size_t, size_t>) {
                        innerKernel(workerIndex, i);
                    }
                    else {
                        innerKernel(i);
                    }
                }
                if(task->isCanceled())
                    break;
                pendingProgress += count;
                if(pendingProgress >= progressUpdateGranularity) {
                    progress.incrementValueNoCancel(pendingProgress);
                    pendingProgress = 0;
                }
            }
        });
        // Flush the progress remaining in this worker's local accumulator.
        if(pendingProgress != 0)
            progress.incrementValueNoCancel(pendingProgress);
    });
}

template<typename OuterKernel>
void parallelForInnerOuter(size_t loopCount, size_t minimumChunkSize, TaskProgress& progress, OuterKernel&& outerKernel)
{
    parallelForInnerOuter(loopCount, minimumChunkSize, progress, [](size_t) noexcept {}, std::forward<OuterKernel>(outerKernel));
}

template<typename Setup, typename Kernel>
void parallelFor(size_t loopCount, size_t minimumChunkSize, TaskProgress& progress, Setup&& setup, Kernel&& kernel)
{
    parallelForInnerOuter(loopCount, minimumChunkSize, progress, std::forward<Setup>(setup),
                                            [kernel = std::forward<Kernel>(kernel)](auto&& iterate) mutable { iterate(kernel); });
}

template<typename Kernel>
void parallelFor(size_t loopCount, size_t minimumChunkSize, TaskProgress& progress, Kernel&& kernel)
{
    parallelFor(loopCount, minimumChunkSize, progress, [](size_t) noexcept {}, std::forward<Kernel>(kernel));
}

/// Runs a parallel loop that produces one partial result object per worker thread and returns the vector
/// of these per-worker results. The \a kernel is invoked as kernel(elementIndex, result), where \a result
/// is the partial result object that this worker accumulates into.
///
/// Each worker accumulates into a worker-local result object that is moved into the returned vector only
/// once the worker has finished its chunk. This avoids false sharing on the densely packed result slots,
/// which would otherwise cause cache-line ping-pong between worker threads on every kernel invocation.
template<typename ResultObject, typename Kernel>
std::vector<ResultObject> parallelForCollect(size_t loopCount, size_t minimumChunkSize, TaskProgress& progress, Kernel&& kernel)
{
    std::vector<ResultObject> results;

    parallelForInnerOuter(loopCount, minimumChunkSize, progress,
        [&](size_t workerCount) { results.resize(workerCount); },
        [&](auto&& iterate) {
            // Accumulate into a worker-local result object kept on this thread's stack, instead of writing
            // directly into the shared results vector on every iteration.
            ResultObject localResult;
            size_t myWorkerIndex = std::numeric_limits<size_t>::max();
            iterate([&](size_t workerIndex, size_t elementIndex) {
                myWorkerIndex = workerIndex;
                kernel(elementIndex, localResult);
            });
            // Hand the completed local result over to its slot in the shared vector. This single write per
            // worker is the only point at which adjacent slots might share a cache line.
            if(myWorkerIndex != std::numeric_limits<size_t>::max())
                results[myWorkerIndex] = std::move(localResult);
        }
    );
    OVITO_ASSERT(!results.empty() || loopCount == 0);

    return results;
}

}   // End of namespace
