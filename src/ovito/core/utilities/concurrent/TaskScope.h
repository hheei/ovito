// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "Task.h"
#include "StopToken.h"
#include "detail/TaskDependency.h"
#include "Launch.h"

namespace Ovito {

/**
 * \brief An owned structured-concurrency scope — the "nursery" tier of the async framework.
 *
 * A TaskScope owns a *stop state* (its own scope task) and a set of child tasks adopted into it.
 * It is the named, first-class form of the owned-scope pattern that MainThreadOperation and the
 * parallelCancellable latch implement by hand (see ARCHITECTURE.md, §4 Tier 1).
 *
 * Lifetime contract (cancel-and-join):
 *  - While alive, the scope installs its scope task as the ambient `this_task::get()`, so work run
 *    inside the scope observes the scope's stopped channel.
 *  - On destruction the scope **requests stop on every child it owns and then joins them** (waits for
 *    each to reach a completion channel) before finishing its own task. No child it adopted outlives
 *    the scope still running.
 *  - A `Bound` scope is bidirectionally cancellation-linked to the enclosing task (parent canceled ⇒
 *    scope canceled, and vice versa); an `Isolated` scope has no parent link.
 *
 * Downward stop propagation to children is implemented with the StopToken/StopCallback facade:
 * each adopted child carries a StopCallback on the scope's token that cancels the child when the scope
 * is stopped.
 *
 * \note The join reuses Task::waitFor(): on the main thread it pumps the event loop while waiting; on a
 *       worker thread it blocks, releasing its pool slot for the duration of the wait so that a
 *       saturated pool cannot deadlock. Like syncWait(), if the scope's own task has *already* been
 *       canceled when the destructor runs, the join returns early rather than waiting. The common case
 *       (a scope ending normally) joins fully.
 *
 * This type is purely additive: adopting a task into a scope is opt-in, so existing async launches are
 * unaffected. The owned-vs-shared distinction (join vs. detach) is formalized by the ScopedFuture
 * coroutine return type, so shared pipeline producers are not erroneously canceled.
 */
class OVITO_CORE_EXPORT TaskScope
{
public:

    enum Kind {
        Isolated, ///< The scope has no parent; it is not cancellation-linked to any enclosing task.
        Bound,    ///< The scope becomes a child of the current task (if any), with bidirectional cancellation.
    };

    /// Creates a scope. By default it is bound to the task currently active in this thread.
    [[nodiscard]] explicit TaskScope(Kind kind = Bound);

    /// Destructor: requests stop on all owned children, joins them, then finishes the scope task.
    ~TaskScope();

    /// A scope is neither copyable nor movable (it installs the ambient task and holds self-pointers).
    TaskScope(const TaskScope&) = delete;
    TaskScope& operator=(const TaskScope&) = delete;

    /// Returns a token observing this scope's stopped channel.
    [[nodiscard]] StopToken get_stop_token() const noexcept { return StopToken(_scopeTask); }

    /// Returns whether cancellation has been requested on this scope.
    [[nodiscard]] bool stop_requested() const noexcept { return _scopeTask->isCanceled(); }

    /// Requests cancellation of this scope (propagates to children and, for a Bound scope, the parent).
    void request_stop() noexcept { _scopeTask->cancel(); }

    /// Returns the scope's own task (its stop state).
    [[nodiscard]] const TaskPtr& task() const noexcept { return _scopeTask; }

    /// Adopts an existing asynchronous result as a child of this scope: it will receive downward stop
    /// propagation while the scope is alive and be joined when the scope is destroyed.
    template<typename FutureType>
    void adopt(const FutureType& child) {
        adoptTask(child.task());
    }

    /// Launches a function as an asynchronous task using the given executor, adopts it as a scope child,
    /// and returns a future for its result.
    template<typename Executor, typename Function>
    [[nodiscard]] auto spawn(Executor&& executor, Function&& function) {
        auto future = launchFunctionAsTask(std::forward<Executor>(executor), std::forward<Function>(function));
        adoptTask(future.task());
        return future;
    }

    /// Launches a function on the worker thread pool, adopts it as a scope child, and returns a future.
    template<typename Function>
    [[nodiscard]] auto spawn(Function&& function) {
        return spawn(ThreadPoolExecutor(_scopeTask->isHighPriorityTask()), std::forward<Function>(function));
    }

private:

    /// Creates the scope's own task, establishing the parent link for a Bound scope.
    static TaskPtr createScopeTask(Kind kind);

    /// Registers the given task as a child of this scope.
    void adoptTask(const TaskPtr& child) {
        OVITO_ASSERT(child);
        _children.emplace_back(detail::TaskDependency(child), get_stop_token());
    }

    /// Function object that cancels a child task; used as the scope's downward stop callback.
    struct CancelChild {
        Task* child;
        void operator()() const noexcept { child->cancel(); }
    };

    /// A child adopted into the scope: a demand reference that keeps it alive for the join, plus a
    /// stop callback that cancels it when the scope is stopped.
    struct Child {
        detail::TaskDependency dep;
        StopCallback<CancelChild> cancelOnScopeStop;

        Child(detail::TaskDependency d, const StopToken& scopeToken) :
            dep(std::move(d)),
            cancelOnScopeStop(scopeToken, CancelChild{dep.get().get()}) {}
    };

    /// The scope's own task — its stop state and the ambient task for work run inside the scope.
    TaskPtr _scopeTask;

    /// Installs the scope task as the current task for the lifetime of the scope.
    Task::Scope _ambient;

    /// The children adopted into this scope. A std::deque keeps element addresses stable (Child is
    /// non-movable because it holds a registered StopCallback).
    std::deque<Child> _children;
};

}   // End of namespace
