// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/Application.h>
#include "Task.h"
#include "Future.h"
#include "detail/TaskCallback.h"

namespace Ovito {

#ifdef OVITO_DEBUG
std::atomic_size_t Task::_globalTaskCounter{0};
#endif

/// A null progress state that ignores all progress reporting calls.
TaskProgress TaskProgress::Ignore{std::nullopt};

/*******************************************************x***********************
* Forwards task status messages to the application instance.
******************************************************************************/
void TaskProgress::logTaskActivity(const QString& progressText)
{
    Application::instance()->logTaskActivity(progressText);
}

#ifdef OVITO_DEBUG
/*******************************************************x***********************
* Destructor.
******************************************************************************/
Task::~Task()
{
    // No-op destructor in release builds.

    // Check if the mutex is currently locked.
    // This should never be the case while destroying the promise state.
    OVITO_ASSERT(_mutex.try_lock());
    _mutex.unlock();

    // At the end of their lifetime, tasks must always end up in the finished state.
    OVITO_ASSERT(isFinished());

    // No continuations should be left.
    OVITO_ASSERT(_continuations.empty());

    // In debug builds we keep track of how many task objects exist to check whether they all get destroyed correctly
    // at program termination.
    _globalTaskCounter.fetch_sub(1);
}
#endif

/******************************************************************************
* Switches the task into the 'finished' state.
******************************************************************************/
void Task::setFinished() noexcept
{
    MutexLock lock(*this);
    if(!(_state.load(std::memory_order_relaxed) & Finished))
        finishLocked(lock);
}

/******************************************************************************
* Puts this task into the 'finished' state (without newly locking the task).
******************************************************************************/
void Task::finishLocked(MutexLock& lock) noexcept
{
    OVITO_ASSERT(lock);

    // Make sure that the result has been set (if not in canceled or error state).
    // Note: This check must happen *before* the task is published as 'finished' below. Once the
    // Finished flag becomes visible, another thread may consume the result (e.g. Future::result()
    // calls takeResult(), which clears _hasResultsStored), which would race with this read and
    // trip the assertion even though the result was set correctly.
    OVITO_ASSERT_MSG(_exceptionStore || isCanceled() || _hasResultsStored.load() || !_resultsStorage,
        "Task::finishLocked()",
        "Result has not been set for the task. Please check program code setting the task to finished.");

    // Put this task into the 'finished' state.
    auto state = _state.fetch_or(Finished, std::memory_order_seq_cst);

    if(!(state & Finished)) {
        // Inform the registered callbacks.
        callCallbacks(Finished, lock);
        OVITO_ASSERT(lock);

        // Note: Move the functions into a new local list first so that we can unlock the mutex.
        decltype(_continuations) continuations = std::move(_continuations);
        OVITO_ASSERT(_continuations.empty());
        lock.unlock();

        // Run all continuation functions.
        for(auto& cont : continuations) {
            std::move(cont)();
        }
    }
}

/******************************************************************************
* Requests cancellation of the task.
******************************************************************************/
void Task::cancel() noexcept
{
    if(!isFinished()) {
        MutexLock lock(*this);
        cancelLocked(lock);
    }
}

/******************************************************************************
* If the task is not finished yet, cancel and finish it.
******************************************************************************/
void Task::cancelAndFinish() noexcept
{
    if(!isFinished()) {
        MutexLock lock(*this);
        cancelLocked(lock);
        finishLocked(lock);
    }
}

/******************************************************************************
* Puts this task into the 'canceled' and 'finished' states (without newly locking the task).
******************************************************************************/
void Task::cancelLocked(MutexLock& lock) noexcept
{
    OVITO_ASSERT(lock);

    // Make sure the task isn't already finished.
    if(_state.load(std::memory_order_relaxed) & Finished)
        return;

    // Set the canceled flag.
    auto state = _state.fetch_or(Canceled, std::memory_order_seq_cst);

    // Inform the registered callbacks that this task got canceled.
    if(!(state & Canceled)) {
        callCallbacks(Canceled, lock);
    }
}

/******************************************************************************
* Puts this task into the 'exception' state to signal that an error has occurred.
******************************************************************************/
void Task::exceptionLocked(std::exception_ptr ex) noexcept
{
    OVITO_ASSERT(ex != std::exception_ptr());

    // Make sure the task isn't already canceled or finished.
    OVITO_ASSERT(!(_state.load(std::memory_order_relaxed) & (Canceled | Finished)));

    _exceptionStore = std::move(ex);
}

/******************************************************************************
* Adds a callback to this task's list, which will get notified during state changes.
******************************************************************************/
void Task::addCallback(detail::TaskCallbackBase* cb, bool replayStateChanges) noexcept
{
    OVITO_ASSERT(cb != nullptr);

    MutexLock lock(*this);

    // Insert into linked list of callbacks.
    cb->_nextInList = _callbacks;
    _callbacks = cb;

    // Replay past state changes to the new callback if requested.
    if(replayStateChanges) {
        cb->callStateChanged(*this, _state.load(std::memory_order_relaxed), lock);
        OVITO_ASSERT(lock.owns_lock());
    }
}

/******************************************************************************
* Invokes the registered callback functions.
******************************************************************************/
void Task::callCallbacks(int state, MutexLock& lock) noexcept
{
    for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList) {
        OVITO_ASSERT(lock.owns_lock());
        cb->callStateChanged(*this, state, lock);
    }
}

/******************************************************************************
* Removes a callback from this task's list, which will no longer get notified about state changes.
******************************************************************************/
void Task::removeCallback(detail::TaskCallbackBase* cb) noexcept
{
    const MutexLock lock(*this);

    // Remove from linked list of callbacks.
    if(_callbacks == cb) {
        _callbacks = cb->_nextInList;
    }
    else {
        for(detail::TaskCallbackBase* cb2 = _callbacks; cb2 != nullptr; cb2 = cb2->_nextInList) {
            if(cb2->_nextInList == cb) {
                cb2->_nextInList = cb->_nextInList;
                return;
            }
        }
        OVITO_ASSERT(false); // Callback was not found in linked list. Did you try to remove a callback that was never added?
    }
}

/******************************************************************************
* Blocks execution until another task finishes.
******************************************************************************/
bool Task::waitFor(detail::TaskDependency awaitedTask, bool throwOnError, bool returnEarlyIfCanceled, bool cancelWaitingIfAwaitedCanceled)
{
    OVITO_ASSERT(awaitedTask);
    OVITO_ASSERT(this_task::get());

    // The task this function was called from.
    Task* waitingTask = this_task::get();
    OVITO_ASSERT_MSG(waitingTask != nullptr, "Task::waitFor()", "No active task. This function may only be called from a context having an active task.");

    // Lock access to the waiting task this function was called by.
    MutexLock waitingTaskLock(*waitingTask);

    // No need to wait for the other task if the waiting task was already canceled.
    if(waitingTask->isCanceled())
        return false;

    // You should never invoke waitFor() from a task that has already finished!
    OVITO_ASSERT(!waitingTask->isFinished());

    // Quick check if the awaited task is already finished or canceled.
    MutexLock awaitedTaskLock(*awaitedTask);
    if(awaitedTask->isCanceled()) {
        // If the awaited task was canceled, cancel the waiting task as well.
        if(cancelWaitingIfAwaitedCanceled)
            waitingTask->cancelLocked(waitingTaskLock);
        // Don't wait for the task to finish.
        if(returnEarlyIfCanceled)
            return false;
    }
    if(awaitedTask->isFinished()) {
        // It's ready, no need to wait.
        if(throwOnError)
            awaitedTask->throwPossibleException();
        return true;
    }

    // Create shared pointers on the stack to make sure the two task objects don't get
    // destroyed during or right after the waiting phase and before we access them again below.
    TaskPtr waitingTaskPtr(waitingTask->shared_from_this());
    TaskPtr awaitedTaskPtr(awaitedTask.get());

    waitingTaskLock.unlock();
    awaitedTaskLock.unlock();

    // Are we running in a thread pool?
    if(!this_task::isMainThread()) {
        // A worker thread is about to block until the awaited task finishes. While it sleeps it must not
        // keep occupying its slot in the thread pool: if every pool thread blocked here at once (each waiting
        // on a task that still needs a pool thread to run), the pool would deadlock. We therefore release the
        // slot for the duration of the wait — QThreadPool::releaseThread() lets the pool temporarily exceed its
        // configured maximum and start (or resume) another worker to make progress — and reserve it again once
        // we wake up. This is Qt's recommended mechanism for a pool thread that goes to sleep waiting; the pool
        // does not expose its internal queue, so genuine work-stealing is not possible. We only do this when the
        // current thread actually belongs to the pool (it may be an unrelated thread, e.g. a std::thread).
        QThreadPool* pool = Application::instance()->taskManager().threadPool();
        if(!pool->contains(QThread::currentThread()))
            pool = nullptr;
        struct ThreadDonationGuard {
            QThreadPool* pool;
            explicit ThreadDonationGuard(QThreadPool* pool) noexcept : pool(pool) { if(pool) pool->releaseThread(); }
            ~ThreadDonationGuard() { if(pool) pool->reserveThread(); }
            Q_DISABLE_COPY_MOVE(ThreadDonationGuard)
        } threadDonationGuard(pool);

        std::condition_variable cv;
        std::mutex waitMutex;
        bool done = false;

        // Attach a temporary callback to the waiting task, which sets the wait condition in case the waiting task gets canceled.
        detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) noexcept {
            if(state & (Task::Canceled | Task::Finished)) {
                // When the parent task gets canceled, discard the task dependency which keeps the awaited task running.
                awaitedTask.reset();
                // Wake up the waiting thread - but only if we shouldn't wait for the task to completely finish.
                if(returnEarlyIfCanceled) {
                    {
                        std::lock_guard lock(waitMutex);
                        done = true;
                    }
                    cv.notify_one();
                }
            }
            return true;
        });

        // Attach a temporary callback function to the awaited task, which sets the wait condition when the task finishes or gets canceled.
        detail::FunctionTaskCallback awaitedTaskCallback(awaitedTaskPtr.get(), [&](int state) noexcept {
            if(state & (returnEarlyIfCanceled ? (Task::Finished | Task::Canceled) : Task::Finished)) {
                {
                    std::lock_guard lock(waitMutex);
                    done = true;
                }
                cv.notify_one();
            }
            return true;
        });

        {
            std::unique_lock lock(waitMutex);
            cv.wait(lock, [&]{ return done; });
        }

        waitingTaskCallback.unregisterCallback();
        awaitedTaskCallback.unregisterCallback();
    }
    else {
        // Process all pending work items while waiting for the task to finish.
        Application::instance()->taskManager().processWorkWhileWaiting(waitingTask, awaitedTask, returnEarlyIfCanceled);
    }

    // Check if the waiting task has been canceled.
    waitingTaskLock.lock();
    if(waitingTask->isCanceled())
        return false;

    // Now check if the awaited task has been canceled.
    awaitedTaskLock.lock();

    if(awaitedTaskPtr->isCanceled()) {
        // If the awaited task was canceled, cancel the waiting task as well.
        if(cancelWaitingIfAwaitedCanceled) {
            waitingTask->cancelLocked(waitingTaskLock);
            return false;
        }
        else {
            return true;
        }
    }

    OVITO_ASSERT(awaitedTaskPtr->isFinished());
    if(throwOnError)
        awaitedTaskPtr->throwPossibleException();

    return true;
}

/******************************************************************************
* Copies the ambient execution context (priority, interactivity, user interface) from the task
* currently active in this thread onto this freshly created task.
******************************************************************************/
void Task::inheritContextFromCurrentTask() noexcept
{
    if(const Task* parentTask = this_task::get()) {
        if(parentTask->isHighPriorityTask())
            setHighPriorityTask();
        if(parentTask->isInteractive())
            setIsInteractive();
        setUserInterface(parentTask->userInterface());
    }
}

namespace this_task {

/*******************************************************x***********************
* Returns the task object that is the active one in the current thread.
******************************************************************************/
Task*& get() noexcept
{
    // The active task in the current thread.
    static thread_local Task* _current = nullptr;

    return _current;
}

/******************************************************************************
* Determines whether the current thread is the main thread of the application.
******************************************************************************/
bool isMainThread() noexcept
{
    OVITO_ASSERT(Application::instance() != nullptr);
    const static QThread* mainThread = Application::instance()->thread();

    return QThread::currentThread() == mainThread;
}

} // End of namespace

} // End of namespace
