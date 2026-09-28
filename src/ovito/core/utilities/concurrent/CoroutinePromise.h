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
#include "Promise.h"
#include "InlineExecutor.h"
#include "LaunchTask.h"
#include "detail/ContinuationTask.h"
#include "detail/FutureDetail.h"

namespace Ovito {

// Forward declaration. Only used in a (constrained) template constructor below, so the complete type is
// not required here — which matters because Core.h includes this header *before* OvitoObject.h.
class OvitoObject;

template<typename R, bool StructuredConcurrency>
class CoroutineTask : public detail::ContinuationTask<R>
{
public:

    /// This is used by the launchTask() utility function.
    using future_type = std::conditional_t<StructuredConcurrency, ScopedFuture<R>, Future<R>>;

    /// Constructor.
    CoroutineTask(std::coroutine_handle<CoroutinePromise<R, StructuredConcurrency>> handle) : _handle(handle) {}

    /// Destructor.
    ~CoroutineTask() {
        // Free the coroutine state if it is still attached to this task.
        if(_handle)
            _handle.destroy();
    }

    /// Detaches the coroutine state from this task after the coroutine has finished running.
    void detachFromCoroutine() noexcept {
        _handle = {};
    }

    /// Resumes the coroutine associated with this task.
    void resumeCoroutine(PromiseBase promise) noexcept {
        OVITO_ASSERT(promise);
        OVITO_ASSERT(_handle);
        OVITO_ASSERT(!_handle.promise());
        OVITO_ASSERT(!this->isFinished());
        static_cast<PromiseBase&>(_handle.promise()) = std::move(promise);
        OVITO_ASSERT(_handle.promise().task().get() == this);
        Task::Scope taskScope(this);
        _handle.resume();
    }

private:

    /// The handle to the coroutine associated with this task.
    std::coroutine_handle<CoroutinePromise<R, StructuredConcurrency>> _handle;
};

template<typename Executor, typename FutureType>
class FutureAwaiter
{
    Q_DISABLE_COPY_MOVE(FutureAwaiter)

public:

    explicit FutureAwaiter(Executor&& executor, FutureType future) noexcept : _executor(std::forward<Executor>(executor)), _future(std::move(future)) {
        OVITO_ASSERT(_future);
    }

    bool await_ready() const noexcept {
        if constexpr(std::is_same_v<std::decay_t<Executor>, InlineExecutor>) {
            return _future.isFinished();
        }
        else {
            return false;
        }
    }

    template<typename R, bool SC>
    void await_suspend(std::coroutine_handle<CoroutinePromise<R, SC>> handle) {
        OVITO_ASSERT(_future);
        auto coroTask = handle.promise().coroTask();
        OVITO_ASSERT(coroTask);
        // Join-vs-detach on this await edge is decided by the awaited child's declared type *alone*,
        // independent of the awaiting coroutine's own return type. A ScopedFuture child has declared
        // "I borrow from my parent's frame across this suspension," so the awaiting frame is kept alive
        // until the child finishes; a plain Future (self-contained by default) and a Tier-2 shared value
        // (a SharedFuture, governed by its demand count) are both detached. The awaiting coroutine's own
        // SC flag concerns a *different* edge — whether it borrows from *its* parent — and must not enter
        // this decision (doing so would force a self-contained producer that merely drives a ScopedFuture
        // sub-operation to mislabel itself as scoped). See ScopedFuture.h and ARCHITECTURE.md §4.3.
        constexpr bool JoinOnCancel = detail::is_scoped_future_v<FutureType>;
        coroTask->template whenTaskFinishes<JoinOnCancel>(_future.takeTaskDependency(), std::move(_executor), std::move(handle.promise()), [this](PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
            _future = FutureType{std::move(finishedTask)};
            auto coroTask = static_cast<CoroutineTask<R, SC>*>(promise.task().get());
            if(!coroTask->isCanceled())
                coroTask->resumeCoroutine(std::move(promise));
        });
    }

    decltype(auto) await_resume() {
        OVITO_ASSERT(_future.isFinished());
        if constexpr(!std::is_same_v<typename FutureType::result_type, void>)
            return std::move(_future).result();
        else
            std::move(_future).takeTaskDependency()->throwPossibleException();
    }

private:

    FutureType _future;
    std::decay_t<Executor> _executor;
};

template<typename Executor>
class ExecutorAwaiter
{
    Q_DISABLE_COPY_MOVE(ExecutorAwaiter)

public:

    explicit ExecutorAwaiter(Executor&& executor) noexcept : _executor(std::forward<Executor>(executor)) {}

    bool await_ready() const noexcept { return false; }

    template<typename R, bool SC>
    void await_suspend(std::coroutine_handle<CoroutinePromise<R, SC>> handle) {
        auto coroTask = handle.promise().coroTask();
        OVITO_ASSERT(coroTask);
        std::move(_executor).execute([promise = std::move(handle.promise())]() mutable noexcept {
            auto coroTask = static_cast<CoroutineTask<R, SC>*>(promise.task().get());
            if(!coroTask->isCanceled())
                coroTask->resumeCoroutine(std::move(promise));
        });
    }

    void await_resume() {}

private:

    std::decay_t<Executor> _executor;
};

template<typename R, bool StructuredConcurrency>
class CoroutinePromiseBase : public PromiseBase
{
public:

    /// Default constructor. The compiler selects it for free-function coroutines and for coroutines that
    /// are member functions of classes *not* derived from OvitoObject.
    CoroutinePromiseBase() noexcept = default;

    /// Automatic self-guard for coroutine *member* functions of OvitoObject-derived classes.
    ///
    /// For a member coroutine the compiler constructs the promise object with the coroutine's arguments,
    /// the first of which is the implicit object parameter (*this). We capture a strong reference to that
    /// object and hold it in the promise — which lives inside the coroutine frame — for the entire
    /// lifetime of the coroutine. This keeps 'this' alive across every suspension point, so a member
    /// coroutine can safely touch its members even after all *external* references to the object have
    /// been dropped while it is still running. See ARCHITECTURE.md §6 (the keep-alive rule).
    ///
    /// When the constraint is not satisfied (free function, value/OORef first parameter, or a non-OvitoObject
    /// class) this constructor drops out of overload resolution and the compiler falls back to the default
    /// constructor above.
    template<typename Self, typename... Args>
        requires std::derived_from<std::remove_cvref_t<Self>, OvitoObject>
    explicit CoroutinePromiseBase(Self& self, Args&&...) noexcept : _selfGuard(self.shared_from_this()) {}

    /// Returns the coroutine task associated with this promise.
    CoroutineTask<R, StructuredConcurrency>* coroTask() noexcept {
        OVITO_ASSERT(*this);
        return static_cast<CoroutineTask<R, StructuredConcurrency>*>(PromiseBase::task().get());
    }

    /// Creates the object that will be returned to the caller of the coroutine.
    std::conditional_t<StructuredConcurrency, ScopedFuture<R>, Future<R>> get_return_object() {
        // Create the task object associated with the coroutine.
        auto coroTask = std::make_shared<CoroutineTask<R, StructuredConcurrency>>(std::coroutine_handle<CoroutinePromise<R, StructuredConcurrency>>::from_promise(static_cast<CoroutinePromise<R, StructuredConcurrency>&>(*this)));
        this->_task = coroTask;
        return launchTask(std::move(coroTask));
    }

    /// Gets called when coroutine throws an exception and nothing catches it.
    void unhandled_exception() noexcept {
        PromiseBase::captureExceptionAndFinish();
    }

    /// Make it an eagerly-started coroutine.
    std::suspend_never initial_suspend() noexcept { return {}; }

    /// Gets called when the coroutine terminates by any means.
    std::suspend_never final_suspend() noexcept {
        OVITO_ASSERT(*this);
        coroTask()->detachFromCoroutine();
        return {};
    }

private:

    /// Strong, type-erased keep-alive reference to the OvitoObject this coroutine is a member of (empty
    /// for free functions and non-OvitoObject classes). Released when the coroutine frame is destroyed.
    /// See the self-guard constructor above.
    std::shared_ptr<const void> _selfGuard;
};

template<typename R, bool StructuredConcurrency>
class CoroutinePromise : public CoroutinePromiseBase<R, StructuredConcurrency>
{
public:
    /// Inherit the base constructors, including the automatic OvitoObject self-guard constructor.
    using CoroutinePromiseBase<R, StructuredConcurrency>::CoroutinePromiseBase;

    /// Sets the result value of the coroutine.
    template<typename R2>
    void return_value(R2&& value) noexcept {
        this->coroTask()->setResult(std::forward<R2>(value));
        PromiseBase::setFinished();
    }
};

template<bool StructuredConcurrency>
class CoroutinePromise<void, StructuredConcurrency> : public CoroutinePromiseBase<void, StructuredConcurrency>
{
public:
    /// Inherit the base constructors, including the automatic OvitoObject self-guard constructor.
    using CoroutinePromiseBase<void, StructuredConcurrency>::CoroutinePromiseBase;

    /// Completes the coroutine.
    void return_void() noexcept {
        PromiseBase::setFinished();
    }
};

template<typename T>
auto operator co_await(Future<T>&& future) noexcept
{
    return FutureAwaiter<InlineExecutor, Future<T>>(InlineExecutor{}, std::move(future));
}

template<typename T>
auto operator co_await(SharedFuture<T>&& future) noexcept
{
    return FutureAwaiter<InlineExecutor, SharedFuture<T>>(InlineExecutor{}, std::move(future));
}

template<typename T>
auto operator co_await(ScopedFuture<T>&& future) noexcept
{
    // Separate overload (selected over the Future<T> one by exact match) so the ScopedFuture marker is
    // preserved into the awaiter, where it drives the join-vs-detach decision for this await edge.
    return FutureAwaiter<InlineExecutor, ScopedFuture<T>>(InlineExecutor{}, std::move(future));
}

/**
 * \brief An opt-in awaiter that requires the awaited handle to be a ScopedFuture (an owned structured child).
 *
 * Semantically identical to FutureAwaiter — it changes no runtime behavior — but it statically rejects a
 * plain Future or a SharedFuture. Use it to make a nested structured (Tier-1) link explicit and
 * self-documenting at the await site, asserting that the awaited child opted into join-on-cancellation.
 * See ARCHITECTURE.md §4.3.
 */
template<typename Executor, typename FutureType>
class ScopedFutureAwaiter : public FutureAwaiter<Executor, FutureType>
{
    static_assert(detail::is_scoped_future_v<FutureType>,
        "ScopedFutureAwaiter requires a ScopedFuture; use FutureAwaiter for a Future or SharedFuture.");

public:

    using FutureAwaiter<Executor, FutureType>::FutureAwaiter;
};

template<typename Executor, typename FutureType>
ScopedFutureAwaiter(Executor&&, FutureType) -> ScopedFutureAwaiter<Executor, FutureType>;

}   // End of namespace
