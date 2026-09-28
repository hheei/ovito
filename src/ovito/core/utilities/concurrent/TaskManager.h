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
#include <ovito/core/oo/OORef.h>
#include <function2/function2.hpp>

namespace Ovito {

/**
 * \brief This class manages execution of asynchronous tasks in a thread pool and
 *        a queue of pending work items to be executed in the main thread.
 */
class OVITO_CORE_EXPORT TaskManager
{
public:

    /// The type-erased function object type to be used for work queue items.
    using work_function_type = fu2::function_base<
        true, // IsOwning = true: The function object owns the callable object and is responsible for its destruction.
        false, // IsCopyable = false: The function object is not copyable.
        fu2::capacity_fixed<4 * sizeof(std::shared_ptr<OvitoObject>)>, // Capacity: Defines the internal capacity of the function for small functor optimization.
        false, // IsThrowing = false: Do not throw an exception on empty function call, call `std::abort` instead.
        true, // HasStrongExceptGuarantee = true: All objects satisfy the strong exception guarantee
        void() noexcept>;

public:

    /// Constructor.
    TaskManager();

#ifdef OVITO_DEBUG
    /// Destructor.
    ~TaskManager();
#endif

    /// Indicates whether the program session is in the process of shutting down.
    bool isShuttingDown() const { return _isShuttingDown; }

#ifdef OVITO_USE_SYCL
    /// Returns the main SYCL out-of-order queue to which work can be submitted.
    sycl::queue& syclQueue() { return _syclQueue; }
#endif

    /// Tells the TaskManager to wait for all in-flight tasks to complete, then shutdown.
    void requestShutdown();

    /// Executes the given function in the main thread at some later time.
    void submitWork(work_function_type&& function);

    /// Returns the thread pool used for executing asynchronous worker tasks.
    QThreadPool* threadPool() { return &_threadPool; }

    /// Submits a runnable to the worker thread pool.
    ///
    /// Low-priority work is enqueued normally with QThreadPool::start(), so it waits for a free thread when the
    /// pool is saturated. High-priority work (the interactive viewport-render subtree) instead oversubscribes the
    /// pool by temporarily raising its thread limit (beginOversubscription()), so it starts running immediately
    /// even when every regular thread is busy with long low-priority work — this is what guarantees timely
    /// interactive rendering without a dedicated second pool. Oversubscription is bounded by the number of
    /// concurrently running high-priority tasks. A high-priority runnable MUST call endOversubscription() once it
    /// has finished running, to restore the thread limit.
    void startWork(QRunnable* runnable, bool highPriority);

    /// Undoes the temporary thread-limit increase that startWork() makes for a high-priority runnable. Must be
    /// called exactly once by each high-priority worker when it finishes (see the runner in ThreadPoolExecutorImpl.h).
    void endOversubscription();

    /// RAII helper that raises the calling worker thread to normal OS scheduling priority while a high-priority
    /// work item runs, restoring the previous (low) priority afterwards. Because a single pool runs both
    /// interactive and background work on reused threads, the priority must be (re)established per runnable.
    /// For regular (low-priority) work this is a no-op: the pool's threads already run at low priority, so the
    /// common case incurs no per-task syscall overhead.
    class OVITO_CORE_EXPORT WorkerThreadPriorityScope
    {
    public:
        explicit WorkerThreadPriorityScope(bool highPriority) noexcept;
        ~WorkerThreadPriorityScope();
        Q_DISABLE_COPY_MOVE(WorkerThreadPriorityScope)
    private:
        bool _active;
        QThread::Priority _previousPriority = QThread::InheritPriority;
    };

    /// Changes the maximum number of threads used by the task manager's thread pool.
    void setMaxThreadCount(int maxThreadCount);

    /// Returns the maximum number of threads used by the task manager's thread pool (the user-configured
    /// limit, excluding any transient oversubscription performed for high-priority work).
    int maxThreadCount() const { return _userMaxThreadCount; }

#ifdef Q_OS_MACOS
    /// Returns whether a native UI dialog (e.g. a QFileDialog) is currently open.
    static bool isNativeDialogActive() { return _nativeDialogActive; }

    /// Informs the task manager that a native UI dialog (e.g. a QFileDialog) is currently open.
    /// During this time, the task manager will not enter a local event loop with
    /// user input event processing on macOS, because this would close the native dialog (for unknown reasons / may be a Qt bug?).
    /// Furthermore, on macOS, we block attempts to the main window during this time, because this destroying the parent of a native dialog can lead to a segfault.
    static void setNativeDialogActive(bool active) { _nativeDialogActive = active; }
#else
    static void setNativeDialogActive(bool) {}
#endif

    /// Executes pending work items waiting in the deferred execution queue.
    void executePendingWork();

private:

    /// Raises the pool's thread limit by one so a high-priority runnable can start immediately. Paired with
    /// endOversubscription(). See startWork().
    void beginOversubscription();

    /// Is called when the pending work queue becomes non-empty.
    /// Precondition: The caller must hold _mutex, because this method accesses shared state
    /// (_eventLoopLocker and _isNotificationEventPending) that may be touched concurrently by the
    /// main thread while a worker thread submits new work via submitWork().
    void notifyWorkArrived();

    /// Executes pending work items waiting in the deferred execution queue.
    void executePendingWorkLocked(std::unique_lock<std::mutex>& lock);

    /// Keeps executing pending work items until quitWorkProcessingLoop() is called or the awaited task has finished.
    void processWorkWhileWaiting(Task* waitingTask, detail::TaskDependency& awaitedTask, bool returnEarlyIfCanceled);

    /// Stops executing pending work items and makes processWorkWhileWaiting() return.
    void quitWorkProcessingLoop(bool& quitFlag, std::optional<QEventLoop>& eventLoop);

    /// Waits for all work queues to become empty.
    void shutdownImplementation(std::unique_lock<std::mutex>& lock);

private:

    /// Indicates whether the task manager is in the process of shutting down.
    bool _isShuttingDown = false;

    /// Indicates that this task manager has completed its shutdown procedure.
    bool _shutdownCompleted = false;

    /// Indicates that a custom event has been posted to the Qt event queue to notify the main event
    /// loop that more work is waiting. Access is guarded by _mutex.
    bool _isNotificationEventPending = false;

    /// Used to keep the Qt main event loop running while the task manager is active.
    std::optional<QEventLoopLocker> _eventLoopLocker;

#ifdef OVITO_USE_SYCL
    /// The main SYCL out-of-order queue for work on the compute device.
    sycl::queue _syclQueue;

    /// The head of the linked list of RegisteredBufferAccess objects associated with this task manager's SYCL queue.
    RegisteredBufferAccess* _registeredBufferAccessors = nullptr;
#endif

    /// The queue of all pending work items that have been submitted for execution in the main thread.
    std::queue<work_function_type> _pendingWork;

    /// Used to signal the arrival of new work items in the main thread queue.
    std::condition_variable _pendingWorkCondition;

    /// Indicates that we are currently waiting for some task to finish in processWorkWhileWaiting().
    TaskPtr _waitingForTask;

    /// Manages thread-safe concurrent access to the work queue.
    std::mutex _mutex;

    /// Guards the thread-limit bookkeeping below (_userMaxThreadCount / _oversubscribeCount and the
    /// corresponding QThreadPool::setMaxThreadCount() calls), so concurrent high-priority submissions and
    /// setMaxThreadCount() calls compose correctly.
    std::mutex _threadLimitMutex;

    /// The user-configured maximum thread count (what maxThreadCount() reports). The pool's actual limit is
    /// this plus _oversubscribeCount.
    int _userMaxThreadCount = 0;

    /// Number of high-priority tasks currently oversubscribing the pool (each adds one to the pool's limit).
    int _oversubscribeCount = 0;

    /// Pool of threads for executing worker tasks. High-priority work oversubscribes this same pool
    /// (see startWork()) rather than using a separate pool.
    QThreadPool _threadPool;

#ifdef Q_OS_MACOS
    /// Indicates that a native UI dialog (e.g. a QFileDialog) is currently open.
    /// During this time, the task manager should not enter a local event loop with
    /// user input event processing, because this would close the native dialog.
    /// This behavior may be a bug in the Qt framework, which has been observed so far on the macOS platform.
    static bool _nativeDialogActive;
#endif

    friend class RegisteredBufferAccess;
    friend class Task; // to call TaskManager::processWorkWhileWaiting() from Task::waitFor()
};

}   // End of namespace
