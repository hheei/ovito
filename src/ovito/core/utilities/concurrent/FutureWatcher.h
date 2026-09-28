// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/utilities/concurrent/QObjectExecutor.h>
#include <ovito/core/utilities/concurrent/detail/FutureDetail.h>

namespace Ovito {

/**
 * \brief Base class for FutureWatcher<FutureType> that provides Qt signals for monitoring
 *        the completion of a Future or SharedFuture.
 *
 * This non-template QObject base holds the Q_OBJECT macro (required by Qt's MOC) and declares
 * the signals that are emitted when the monitored future reaches its final state.
 */
class OVITO_CORE_EXPORT FutureWatcherBase : public QObject
{
    Q_OBJECT

protected:

    /// Constructor.
    explicit FutureWatcherBase(QObject* parent = nullptr) : QObject(parent) {
        OVITO_ASSERT(this_task::isMainThread());
    }

Q_SIGNALS:

    /// This signal is emitted when the future completes successfully (not canceled, no exception).
    void completed();

    /// This signal is emitted when the future is canceled before completion.
    void canceled();

    /// This signal is emitted when the future completes with an exception.
    void error(const Ovito::Exception& exception);

    /// This signal is emitted when the watcher has finished monitoring a future and no new future has been set by the other signals' handlers.
    /// Note that if a new future is set in the completed/canceled/error signal handlers, the watcher will immediately start monitoring the new future
    /// and the finishedWatching() signal will not be emitted until that future also reaches its final state.
    void finishedWatching();

protected:

    /// Called by the template subclass when the monitored future has reached the
    /// finished state. Determines the outcome and emits the appropriate signals.
    void futureFinished(Task& task) noexcept;
};

/**
 * \brief Monitors the completion of a Future<T> or SharedFuture<T> and emits Qt signals
 *        when the future reaches its final state.
 *
 * The FutureWatcher installs a continuation on the monitored future via finally(), using
 * a QObjectExecutor to ensure that signals are always emitted in the main thread, regardless
 * of which thread the future completes in. The outcome is decoded into the
 * finished()/completed()/canceled()/error() signals via detail::decodeOutcome (in FutureWatcher.cpp).
 *
 * Note: This shares the identity-guarded self-reset pattern of OperationSlot, but is deliberately NOT
 * built on it. It keeps the monitored task in a strong TaskPtr (_task) that survives independently of
 * the consumer-facing future (_future) for the whole duration of signal emission: a signal handler may
 * reset the future (e.g. via requestCancelation()) while a later handler still calls result() to read
 * the task's result storage. OperationSlot only models the consumer-facing future, so it cannot express
 * this "keep the task alive across signal emission" requirement.
 *
 * Usage example:
 * \code
 *   auto watcher = new FutureWatcher<SharedFuture<QString>>(someSharedFuture, this);
 *   connect(watcher, &FutureWatcherBase::completed, this, [watcher] {
 *       qDebug() << watcher->result();
 *   });
 * \endcode
 */
template<typename FutureType>
class FutureWatcher : public FutureWatcherBase
{
public:

    /// The result type of the monitored future.
    using result_type = typename FutureType::result_type;

    /// Creates an uninitialized watcher not yet associated with any future.
    explicit FutureWatcher(QObject* parent = nullptr) : FutureWatcherBase(parent) {}

    /// Creates a watcher and immediately begins monitoring the given future.
    explicit FutureWatcher(FutureType future, QObject* parent = nullptr) : FutureWatcherBase(parent) {
        setFuture(std::move(future));
    }

    /// Associates this watcher with the given future and begins monitoring it.
    /// Must be called from the main thread.
    void setFuture(FutureType future) {
        OVITO_ASSERT(this_task::isMainThread());
        _future = std::move(future);
        _task = _future.task();
        if(_task) {
            _task->finally(QObjectExecutor(this), [this](Task& task) noexcept {
                // Ignore stale callbacks from previously replaced futures.
                if(_task.get() != &task)
                    return;
                _future.reset();
                futureFinished(task);
                if(_task.get() == &task) {
                    _task.reset();
                    OVITO_ASSERT(!_future);
                    Q_EMIT finishedWatching();
                }
            });
        }
    }

    /// Returns whether this watcher is currently monitoring a future.
    bool hasFuture() const {
        OVITO_ASSERT(this_task::isMainThread());
        return (bool)_future;
    }

    /// Requests cancelation of the future by disassociating the watcher from the monitored future.
    /// Must be called from the main thread.
    /// Note: If the monitored future is a SharedFuture, the future may still reach the completed state
    /// and the completed() signal may still be emitted.
    void requestCancelation() {
        OVITO_ASSERT(this_task::isMainThread());
        _future.reset();
    }

    /// Blocks the calling (GUI) thread until the monitored future has reached a final state.
    /// Throws an OperationCanceled exception if the future or the task awaiting it got canceled.
    /// Throws an exception if the awaited task has failed to complete.
    /// Note: This method should only be used in exceptional cases, e.g. from Python code that
    /// cannot use asynchronous continuations. In general, blocking the GUI thread should be avoided.
    void blockUntilFinished() {
        OVITO_ASSERT(this_task::isMainThread());
        if(_future)
            _future.waitForFinished();
    }

    /// Returns the result of the successfully completed future.
    /// For Future<T>: returns the result by value (moves it out).
    /// For SharedFuture<T>: returns a const reference.
    /// This method may only be called from the completed() signal handler.
    [[nodiscard]] auto result()
        requires (!std::is_void_v<result_type>)
    {
        OVITO_ASSERT(this_task::isMainThread());
        OVITO_ASSERT(_task);
        OVITO_ASSERT(_task->isFinished());
        OVITO_ASSERT(!_task->isCanceled());
        OVITO_ASSERT(!_task->exceptionStore());

        if constexpr(detail::is_shared_future_v<FutureType>) {
            return _task->template getResult<result_type>();  // Returns const result_type&
        }
        else {
            return _task->template takeResult<result_type>();  // Returns result_type by move
        }
    }

private:

    /// The monitored future.
    FutureType _future;

    /// The task of the monitored future, kept alive independently of (and longer than) _future so that
    /// result() works throughout signal emission even if a handler resets _future first.
    TaskPtr _task;
};

}   // End of namespace
