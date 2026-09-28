// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/dataset/data/RegisteredBufferAccess.h>

namespace Ovito {

#ifdef Q_OS_MACOS
/// Indicates that a native UI dialog (e.g. a QFileDialog) is currently open.
bool TaskManager::_nativeDialogActive = false;
#endif

/******************************************************************************
* Raises the calling worker thread to normal priority while high-priority work runs.
******************************************************************************/
TaskManager::WorkerThreadPriorityScope::WorkerThreadPriorityScope(bool highPriority) noexcept : _active(highPriority)
{
    if(_active) {
        // Remember the thread's current (low) priority and switch to normal priority for the duration of the work.
        _previousPriority = QThread::currentThread()->priority();
        QThread::currentThread()->setPriority(QThread::NormalPriority);
    }
}

/******************************************************************************
* Restores the worker thread's previous priority.
******************************************************************************/
TaskManager::WorkerThreadPriorityScope::~WorkerThreadPriorityScope()
{
    // Only restore if we actually changed the priority and the previous value is a settable priority
    // (QThread::priority() reports InheritPriority for a thread whose priority was never explicitly set).
    if(_active && _previousPriority != QThread::InheritPriority)
        QThread::currentThread()->setPriority(_previousPriority);
}

/******************************************************************************
* Submits a runnable to the worker thread pool.
******************************************************************************/
void TaskManager::startWork(QRunnable* runnable, bool highPriority)
{
    if(highPriority) {
        // Oversubscribe the pool so the high-priority runnable starts immediately, even if all regular
        // threads are currently busy with long-running low-priority work. beginOversubscription() raises the
        // pool's hard thread limit by one, giving start() a free slot to spawn a worker for the runnable. The
        // limit must stay raised until the runnable has actually run (QThreadPool spawns the worker
        // asynchronously, so lowering the limit right away would cancel the extra slot before the worker is
        // created); the high-priority worker therefore calls endOversubscription() when it finishes (see the
        // runner in ThreadPoolExecutorImpl.h), restoring the limit. The two deltas net to zero per task.
        //
        // (We deliberately do not use reserveThread()/releaseThread()/startOnReservedThread() here: those only
        // adjust QThreadPool's reservation bookkeeping and do not reliably spawn a worker on a long-lived,
        // already-saturated pool — verified by the unit tests in tst_concurrent_pool.)
        beginOversubscription();
        _threadPool.start(runnable, /*priority*/ 1);
    }
    else {
        _threadPool.start(runnable);
    }
}

/******************************************************************************
* Temporarily raises the pool's thread limit by one for a high-priority task.
******************************************************************************/
void TaskManager::beginOversubscription()
{
    std::lock_guard<std::mutex> lock(_threadLimitMutex);
    _oversubscribeCount++;
    _threadPool.setMaxThreadCount(_userMaxThreadCount + _oversubscribeCount);
}

/******************************************************************************
* Undoes a previous beginOversubscription(); called by a high-priority worker when it finishes.
******************************************************************************/
void TaskManager::endOversubscription()
{
    std::lock_guard<std::mutex> lock(_threadLimitMutex);
    OVITO_ASSERT(_oversubscribeCount > 0);
    _oversubscribeCount--;
    _threadPool.setMaxThreadCount(_userMaxThreadCount + _oversubscribeCount);
}

/******************************************************************************
* Changes the maximum number of threads used by the task manager's thread pool.
******************************************************************************/
void TaskManager::setMaxThreadCount(int maxThreadCount)
{
    std::lock_guard<std::mutex> lock(_threadLimitMutex);
    _userMaxThreadCount = maxThreadCount;
    _threadPool.setMaxThreadCount(_userMaxThreadCount + _oversubscribeCount);
}

/******************************************************************************
* Initializes the task manager.
******************************************************************************/
TaskManager::TaskManager()
#ifdef OVITO_USE_SYCL
    , _syclQueue{sycl::default_selector_v}
#endif
{
    // Track the pool's initial (default) thread limit, which is QThread::idealThreadCount().
    _userMaxThreadCount = _threadPool.maxThreadCount();

    // Run regular work tasks with reduced priority to avoid slowing down the user interface.
    _threadPool.setThreadPriority(QThread::LowPriority);

    // Use all available processor cores by default -- or the user-specified
    // number given by the OVITO_THREAD_COUNT environment variable.
    if(int threadCount = qEnvironmentVariableIntValue("OVITO_THREAD_COUNT"))
        setMaxThreadCount(std::max(1, threadCount));
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
TaskManager::~TaskManager()
{
    OVITO_ASSERT(isShuttingDown());
    OVITO_ASSERT(_shutdownCompleted);
    OVITO_ASSERT(!_eventLoopLocker.has_value());

    // Check if the mutex is currently locked.
    // This should never be the case while destroying the TaskManager.
    OVITO_ASSERT(_mutex.try_lock());
    _mutex.unlock();
}
#endif

/******************************************************************************
* Tells the TaskManager to wait for all in-flight tasks to complete, then shutdown.
******************************************************************************/
void TaskManager::requestShutdown()
{
    OVITO_ASSERT(this_task::isMainThread());

    // Set flag to indicate we are shutting down.
    std::unique_lock<std::mutex> lock(_mutex);
    if(isShuttingDown())
        return;
    _isShuttingDown = true;

    // Wait for all remaining tasks to finish and proceed with shutdown as soon as control has returned to main event loop.
    if(QCoreApplication::instance() && QThread::currentThread()->loopLevel() != 0) {
        // notifyWorkArrived() requires the mutex to be held.
        notifyWorkArrived();
        lock.unlock();
    }
    else {
        // executePendingWork() acquires the mutex itself, so release it first.
        lock.unlock();
        executePendingWork();
    }
}

/******************************************************************************
* Waits for all work queues to become empty.
******************************************************************************/
void TaskManager::shutdownImplementation(std::unique_lock<std::mutex>& lock)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(isShuttingDown());
    OVITO_ASSERT(!_waitingForTask);

    // Work item queue must be empty by now.
    OVITO_ASSERT(_pendingWork.empty());
    lock.unlock();

    // Remove obsolete notification events from Qt event queue.
    if(QCoreApplication::instance())
        QCoreApplication::sendPostedEvents(Application::instance());

    // Wait until all threads did terminate. That's because canceled asynchronous tasks
    // may still be running in threads until they notice they have been canceled.
    Q_DECL_UNUSED bool result = _threadPool.waitForDone();
    OVITO_ASSERT(result);

    // Shuts down the SYCL queue.
#ifdef OVITO_USE_SYCL
    // Close all active SYCL host memory accessors which are associated with NumPy views of properties.
    for(RegisteredBufferAccess* accessor = _registeredBufferAccessors; accessor != nullptr; accessor = accessor->_next) {
        accessor->_syclAccessor = {};
    }

    // Wait for completion of all enqueued tasks in the SYCL queue.
    _syclQueue.wait();
#endif

    lock.lock();

    OVITO_ASSERT(_threadPool.activeThreadCount() == 0);

    // After the thread pools have been shut down, more work items may have been added to the main thread's queue.
    // First, execute these items before proceeding with the shutdown process.
    if(!_pendingWork.empty()) {
        executePendingWorkLocked(lock);
        return;
    }

    _shutdownCompleted = true;
    _eventLoopLocker.reset();
    lock.unlock();
}

/******************************************************************************
* Executes the given function in the main thread at some later time.
******************************************************************************/
void TaskManager::submitWork(work_function_type&& function)
{
    // Place work item into the queue.
    // Note: This method may be called from a worker thread (e.g. via ObjectExecutor). Access to the
    // queue and to the state touched by notifyWorkArrived() must therefore be synchronized with the
    // main thread, which concurrently drains the queue.
    std::unique_lock<std::mutex> lock(_mutex);
    OVITO_ASSERT(!_shutdownCompleted);
    _pendingWork.push(std::move(function));

    // If the queue became non-empty, notify listeners that work is waiting.
    if(_pendingWork.size() == 1) {
        // notifyWorkArrived() must be called while still holding the mutex.
        notifyWorkArrived();
        lock.unlock();
        _pendingWorkCondition.notify_one();
    }
}

/******************************************************************************
* Is called when the pending work queue becomes non-empty
******************************************************************************/
void TaskManager::notifyWorkArrived()
{
    // The caller must hold _mutex (see declaration), because the state accessed here is shared with
    // the main thread that drains the work queue.
    if(QCoreApplication::instance() && !_isNotificationEventPending) {

        // Keep the Qt main event loop running while the task manager's work queue is non-empty.
        if(!_eventLoopLocker.has_value())
            _eventLoopLocker.emplace();

        // Place a custom event in the Qt event queue to trigger work processing as soon as control
        // returns to the event loop.
        // The custom event type's destructor runs the work processing function.
        struct Event : public QEvent {
            Event() : QEvent(QEvent::None) {}
            ~Event() { Application::instance()->taskManager().executePendingWork(); }
        };
        _isNotificationEventPending = true;
        QCoreApplication::postEvent(Application::instance(), new Event());
    }
}

/******************************************************************************
* Executes pending work items waiting in the queue.
******************************************************************************/
void TaskManager::executePendingWork()
{
    std::unique_lock<std::mutex> lock{_mutex};
    _isNotificationEventPending = false;
    executePendingWorkLocked(lock);
}

/******************************************************************************
* Executes pending work items waiting in the queue.
******************************************************************************/
void TaskManager::executePendingWorkLocked(std::unique_lock<std::mutex>& lock)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(!_shutdownCompleted || _pendingWork.empty());

    size_t itemCount = _pendingWork.size();
    while(!_pendingWork.empty()) {
        {
            // Grab the next work item from the queue.
            auto work = std::move(_pendingWork.front());
            _pendingWork.pop();
            lock.unlock();

            // Provide a clean task environment for the work.
            Task::Scope taskScope(nullptr);
            // Temporarily disable undo recording while in the event loop.
            UndoSuspender noUndo;

            // Execute the work function.
            std::move(work)();

            // Note: Work item is destructed here at scope exit
            // and must happen while task manager's mutex is unlocked.
            // That's because the work's destructor might trigger further actions
            // that submit new work to the queue.
        }

        // Continue by grabbing the next work item from the queue.
        lock.lock();

        // Make sure to return control to the GUI event loop before processing any newly queued work items.
        itemCount--;
        if(itemCount == 0 && !_pendingWork.empty() && QCoreApplication::instance()) {
            notifyWorkArrived();
            return;
        }
    }

    if(isShuttingDown() && !_waitingForTask && !_shutdownCompleted) {
        // If shutdown has been requested before, and we now have left all nested loops,
        // wait for all remaining tasks (in other threads) to finish.
        shutdownImplementation(lock);
    }

    // Discard QEventLoopLocker after all waiting work items have been processed.
    _eventLoopLocker.reset();
}

/******************************************************************************
* Keeps executing pending work items until quitWorkProcessingLoop() is called
* or the awaited task has finished.
******************************************************************************/
void TaskManager::processWorkWhileWaiting(Task* waitingTask, detail::TaskDependency& awaitedTask, bool returnEarlyIfCanceled)
{
    // This method must only be used in the main thread.
    OVITO_ASSERT(this_task::isMainThread());

    std::unique_lock<std::mutex> lock(_mutex);
    TaskPtr wasWaitingForTask = std::exchange(_waitingForTask, awaitedTask.get());
    bool quitFlag = false;

    std::optional<QEventLoop> eventLoop;

    lock.unlock();
    // Register a callback function with the awaited task, which terminates the processing loop when the task gets canceled or finishes.
    detail::FunctionTaskCallback awaitedTaskCallback(_waitingForTask.get(), [&](int state) noexcept {
        if(state & (returnEarlyIfCanceled ? (Task::Finished | Task::Canceled) : Task::Finished)) {
            quitWorkProcessingLoop(quitFlag, eventLoop);
        }
        return true;
    });

    // Register a callback function with the waiting task, which terminates the processing loop in case the waiting task gets canceled.
    detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) noexcept {
        if(state & (Task::Canceled | Task::Finished)) {
            // When the parent task gets canceled, discard the task dependency which keeps the awaited task running.
            awaitedTask.reset();
            // Stop waiting - but only if we shouldn't wait for the task to completely finish.
            if(returnEarlyIfCanceled) {
                quitWorkProcessingLoop(quitFlag, eventLoop);
            }
        }
        return true;
    });
    lock.lock();

    for(;;) {

        // Time to quit the loop because an interrupt was requested?
        if(quitFlag)
            break;

        // Create a local event loop if a Qt application has been set up.
        if(QCoreApplication::instance() && !eventLoop) {
            eventLoop.emplace();
        }

        // Temporarily switch back to a null context while in the event loop.
        Task::Scope taskScope(nullptr);
        // Temporarily disable undo recording while in the event loop.
        UndoSuspender noUndo;

        // Is the Qt main event loop running? If yes, start a local Qt event loop, which processes both Qt events and pending work items.
        // Otherwise, in a non-GUI environment, enter into our own processing loop, which only processes pending work items of this task manager.
        if(eventLoop) {
            // First, process remaining items in the work queue until the queue is drained.
            executePendingWorkLocked(lock);
            if(quitFlag)
                break;

            // Enter the local Qt event loop.
            lock.unlock();
#ifndef Q_OS_MACOS
            eventLoop->exec();
#else
            /// If a native UI dialog (e.g. a QFileDialog) is currently open,
            /// do not start a local event loop with user input event processing.
            /// For unknown reasons, this closes the native dialog upon loop exit.
            eventLoop->exec(isNativeDialogActive() ? QEventLoop::ExcludeUserInputEvents : QEventLoop::AllEvents);
#endif
            lock.lock();

            if(_waitingForTask->isCanceled() || _waitingForTask->isFinished() || waitingTask->isCanceled())
                break;
        }
        else {
            // Block until new work arrives in the queue or quitWorkProcessingLoop() is called.
            _pendingWorkCondition.wait(lock, [&]{
                return !_pendingWork.empty() || quitFlag;
            });

            // Time to quit the loop because we are done?
            if(quitFlag)
                break;

            // Process newly arrived items in the work queue until the queue is drained.
            executePendingWorkLocked(lock);
        }
    }

    // Detach callbacks from the two task objects.
    waitingTaskCallback.unregisterCallback();
    awaitedTaskCallback.unregisterCallback();
    _waitingForTask.swap(wasWaitingForTask);

    // Make sure the awaited task really has finished running by now if the caller requested that.
    OVITO_ASSERT(returnEarlyIfCanceled || wasWaitingForTask->isFinished());

    // Make sure the awaited task really has finished or was canceled by now - or the waiting task has been canceled at least.
    OVITO_ASSERT(wasWaitingForTask->isCanceled() || wasWaitingForTask->isFinished() || waitingTask->isCanceled());

    // If shutdown has been requested before, and we now have left all nested loops,
    // wait for all remaining tasks to finish and proceed with shutdown process as soon
    // as control has returned to main event loop.
    if(isShuttingDown() && !_waitingForTask) {
        if(QCoreApplication::instance() && QThread::currentThread()->loopLevel() != 0)
            notifyWorkArrived();
        else
            executePendingWorkLocked(lock);
    }
}

/******************************************************************************
* Stops executing pending work items and makes processWorkWhileWaiting() return.
******************************************************************************/
void TaskManager::quitWorkProcessingLoop(bool& quitFlag, std::optional<QEventLoop>& eventLoop)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if(_waitingForTask) {
        quitFlag = true;
        if(eventLoop) {
            eventLoop->quit();
        }
        else {
            _pendingWorkCondition.notify_one();
        }
    }
}

}   // End of namespace
