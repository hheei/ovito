// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Phase-0 safety net for the asynchronous task framework
// (ovito/src/ovito/core/utilities/concurrent/). See ARCHITECTURE.md in that directory
// for the conceptual model this suite pins down.
//
// These tests deliberately exercise only the parts of the framework that run without an
// Application/TaskManager instance: the Promise/Future/SharedFuture state machine, the
// InlineExecutor continuation path (then/finally), demand-counted cancellation, the
// this_task ambient-task helpers, and the non-blocking quick paths of Task::waitFor().
// They lock the current observable behavior of the value/error/stopped completion channels
// before the structured-concurrency refactors (StopToken, TaskScope, ...) begin.
//

#include <QTest>
#include <ovito/core/Core.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/utilities/concurrent/StopToken.h>
#include <ovito/core/utilities/concurrent/TaskScope.h>
#include <ovito/core/utilities/concurrent/SharedAsyncValue.h>
#include <ovito/core/utilities/concurrent/OperationSlot.h>
#include <ovito/core/utilities/concurrent/InlineExecutor.h>
#include <ovito/core/utilities/concurrent/detail/TaskOutcome.h>
#include <ovito/core/utilities/concurrent/ScopedFuture.h>
#include <ovito/core/utilities/concurrent/CoroutinePromise.h>
#include <ovito/core/utilities/concurrent/detail/TaskDependency.h>
#include <ovito/core/utilities/concurrent/detail/FutureDetail.h>

using namespace Ovito;

namespace {

// A coroutine whose return type is ScopedFuture<R> opts into Tier-1 owned-scope semantics
// (see Phase 4). These helpers stay fully inline (eager body, only finished awaited values),
// so they run to completion synchronously and need no TaskManager / thread pool.

ScopedFuture<int> scopedCoroReturn(int x) {
    co_return x * 2;
}

ScopedFuture<int> scopedCoroAwaitValue(Future<int> input) {
    int v = co_await std::move(input);
    co_return v + 1;
}

ScopedFuture<int> scopedCoroAwaitShared(SharedFuture<int> input) {
    int v = co_await std::move(input);
    co_return v + 1;
}

// A minimal OvitoObject used to verify the automatic self-guard that CoroutinePromiseBase installs for
// coroutine *member* functions (ARCHITECTURE.md §6): the coroutine must keep 'this' alive across a
// suspension point even after every external reference to the object has been dropped.
class SelfGuardTestObject : public OvitoObject
{
public:
    using OvitoObject::OvitoObject;

    /// A data member the coroutine reads on resumption; reading it is only valid if 'this' is still alive.
    int memberValue = 12345;

    /// A coroutine member function: it suspends on the awaited input, then returns a value computed from
    /// the data member. It deliberately carries NO manual "OORef<Self> self(this)" self-guard, relying
    /// entirely on the automatic one installed by the promise constructor.
    Future<int> addMember(Future<int> input) {
        int v = co_await std::move(input);
        co_return v + memberValue;
    }
};

// A type without a default constructor, used to verify that task result storage does not require
// the result type to be default-constructible.
struct NoDefault {
    int value;
    explicit NoDefault(int v) noexcept : value(v) {}
    NoDefault() = delete;
    NoDefault(NoDefault&&) noexcept = default;
    NoDefault(const NoDefault&) noexcept = default;
};

} // anonymous namespace

class ConcurrentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // Completion channels: value / error / stopped
    // -----------------------------------------------------------------------

    void channel_value() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();
        QVERIFY(static_cast<bool>(f));
        QVERIFY(!f.isFinished());
        QVERIFY(!f.isCanceled());

        p.setResult(42);
        p.setFinished();

        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());
        QVERIFY(!f.getExceptionIfFailed().has_value());
        QCOMPARE(f.result(), 42);  // consumes the future
        QVERIFY(!static_cast<bool>(f));
    }

    void channel_error() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        try { throw Exception(QStringLiteral("boom")); }
        catch(...) { p.captureExceptionAndFinish(); }

        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());  // an error is NOT a cancellation

        // The error channel is observable both as a stored exception ...
        std::optional<Exception> ex = f.getExceptionIfFailed();
        QVERIFY(ex.has_value());

        // ... and as a C++ exception rethrown by result().
        bool threw = false;
        try { (void)f.result(); }
        catch(const Exception&) { threw = true; }
        QVERIFY(threw);
    }

    void channel_stopped() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        p.cancel();
        // cancel() sets the stopped channel but does not by itself finish the task.
        QVERIFY(f.isCanceled());
        QVERIFY(!f.isFinished());

        // Driving the task to completion keeps it canceled and marks it finished.
        p.reset();  // cancelAndFinish()
        QVERIFY(f.isCanceled());
        QVERIFY(f.isFinished());
        // A canceled task reports no error (stopped != error).
        QVERIFY(!f.getExceptionIfFailed().has_value());
    }

    void immediate_value_future() {
        Future<int> f = Future<int>::createImmediate(123);
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());
        QCOMPARE(f.result(), 123);
    }

    // -----------------------------------------------------------------------
    // Result storage supports types that are not default-constructible: the
    // TaskWithStorage result is held in a union and constructed in place only
    // when setResult() runs (see detail/TaskWithStorage.h).
    // -----------------------------------------------------------------------

    void non_default_constructible_result_value_channel() {
        Promise<NoDefault> p = Promise<NoDefault>::create();   // result storage left uninitialized
        Future<NoDefault> f = p.future();

        p.setResult(NoDefault{42});                            // constructs the result in place
        p.setFinished();

        QVERIFY(f.isFinished());
        QCOMPARE(f.result().value, 42);
    }

    void non_default_constructible_result_immediate() {
        Future<NoDefault> f = Future<NoDefault>::createImmediate(NoDefault{7});
        QVERIFY(f.isFinished());
        QCOMPARE(f.result().value, 7);
    }

    void non_default_constructible_result_stopped_without_value() {
        // A task whose result was never constructed must not try to destroy one (the union is
        // destroyed only when _resultsStorage indicates the result was constructed).
        Promise<NoDefault> p = Promise<NoDefault>::create();
        Future<NoDefault> f = p.future();
        p.cancel();
        p.reset();   // cancelAndFinish(); dropping the task must not destruct an unconstructed result
        QVERIFY(f.isCanceled());
        QVERIFY(f.isFinished());
    }

    // -----------------------------------------------------------------------
    // Demand-counted cancellation (TaskDependency / _dependentsCount)
    // -----------------------------------------------------------------------

    void demand_drop_last_dependency_cancels() {
        Promise<int> p = Promise<int>::create();
        const TaskPtr task = p.task();  // keep the Task object alive independently of the future
        QVERIFY(!task->isCanceled());
        {
            Future<int> f = p.future();
            QVERIFY(!task->isCanceled());
        }
        // Dropping the last consumer (demand -> 0) requests stop.
        QVERIFY(task->isCanceled());
        QVERIFY(!task->isFinished());
    }

    void demand_shared_kept_alive_until_last_consumer() {
        Promise<int> p = Promise<int>::create();
        const TaskPtr task = p.task();

        SharedFuture<int> f1 = p.sharedFuture();
        SharedFuture<int> f2 = f1;  // second consumer: demand == 2

        f1.reset();  // demand 2 -> 1
        QVERIFY(!task->isCanceled());

        f2.reset();  // demand 1 -> 0
        QVERIFY(task->isCanceled());
    }

    // -----------------------------------------------------------------------
    // Fan-out: a SharedFuture replays its result to every consumer
    // -----------------------------------------------------------------------

    void shared_future_fans_out_value() {
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f1 = p.sharedFuture();
        SharedFuture<int> f2 = p.sharedFuture();

        p.setResult(7);
        p.setFinished();

        // Both consumers can read the (non-consuming) result.
        QCOMPARE(f1.result(), 7);
        QCOMPARE(f2.result(), 7);
    }

    // -----------------------------------------------------------------------
    // Continuations: then() via the InlineExecutor
    // -----------------------------------------------------------------------

    void then_runs_after_source_finishes() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        int runs = 0;
        Future<int> g = f.then(InlineExecutor{}, [&](int x) { runs++; return x * 2; });

        // The continuation is deferred until the source reaches the value channel.
        QCOMPARE(runs, 0);
        QVERIFY(!g.isFinished());

        p.setResult(21);
        p.setFinished();

        QCOMPARE(runs, 1);
        QVERIFY(g.isFinished());
        QCOMPARE(g.result(), 42);
    }

    void then_skips_continuation_on_error_and_forwards_it() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        bool ran = false;
        Future<int> g = f.then(InlineExecutor{}, [&](int x) { ran = true; return x + 1; });

        try { throw Exception(QStringLiteral("source failed")); }
        catch(...) { p.captureExceptionAndFinish(); }

        QVERIFY(g.isFinished());
        QVERIFY(!ran);  // a value-taking continuation is skipped when the source errored
        QVERIFY(g.getExceptionIfFailed().has_value());  // the error is forwarded downstream
    }

    void then_captures_continuation_exception() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        Future<int> g = f.then(InlineExecutor{}, [](int) -> int {
            throw Exception(QStringLiteral("continuation failed"));
        });

        p.setResult(1);
        p.setFinished();

        QVERIFY(g.isFinished());
        QVERIFY(!g.isCanceled());
        QVERIFY(g.getExceptionIfFailed().has_value());
    }

    // -----------------------------------------------------------------------
    // Continuations: finally() always runs
    // -----------------------------------------------------------------------

    void finally_runs_on_value() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        bool ran = false;
        f.finally(InlineExecutor{}, [&]() noexcept { ran = true; });
        QVERIFY(!ran);

        p.setResult(1);
        p.setFinished();
        QVERIFY(ran);
    }

    void finally_runs_on_cancellation() {
        Promise<int> p = Promise<int>::create();
        Future<int> f = p.future();

        bool ran = false;
        bool sawCanceled = false;
        f.finally(InlineExecutor{}, [&](Task& t) noexcept { ran = true; sawCanceled = t.isCanceled(); });

        p.reset();  // cancelAndFinish()
        QVERIFY(ran);
        QVERIFY(sawCanceled);
    }

    // -----------------------------------------------------------------------
    // detail::decodeOutcome — classifies a finished task into its terminal channel
    // -----------------------------------------------------------------------

    void decode_outcome_completed() {
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f = p.sharedFuture();
        p.setResult(1);
        p.setFinished();
        QCOMPARE(detail::decodeOutcome(*f.task()), detail::TaskOutcome::Completed);
    }

    void decode_outcome_canceled() {
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f = p.sharedFuture();
        p.cancel();
        p.setFinished();
        QCOMPARE(detail::decodeOutcome(*f.task()), detail::TaskOutcome::Canceled);
    }

    void decode_outcome_failed() {
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f = p.sharedFuture();
        try { throw Exception(QStringLiteral("boom")); }
        catch(...) { p.captureExceptionAndFinish(); }
        QCOMPARE(detail::decodeOutcome(*f.task()), detail::TaskOutcome::Failed);
    }

    void decode_outcome_cancellation_dominates_exception() {
        // A task that carries both a stored exception and the canceled flag classifies as Canceled,
        // matching Task::getExceptionIfFailed() (the channel-priority policy decodeOutcome encodes).
        auto task = std::make_shared<Task>();
        try { throw Exception(QStringLiteral("boom")); }
        catch(...) { task->captureException(); }
        task->cancel();
        task->setFinished();
        QVERIFY(task->exceptionStore());
        QCOMPARE(detail::decodeOutcome(*task), detail::TaskOutcome::Canceled);
    }

    // -----------------------------------------------------------------------
    // OperationSlot — owner-held single in-flight operation with identity-guarded self-management
    // -----------------------------------------------------------------------

    void operation_slot_auto_resets_on_finish() {
        OperationSlot<int> slot;
        Promise<int> p = Promise<int>::create();
        slot.startAutoReset(p.sharedFuture(), InlineExecutor{});
        QVERIFY(slot);
        QVERIFY(!slot.isFinished());

        p.setResult(7);
        p.setFinished();
        // The slot cleared itself once its operation finished.
        QVERIFY(!slot);
    }

    void operation_slot_runs_continuation_on_finish() {
        OperationSlot<int> slot;
        Promise<int> p = Promise<int>::create();
        Task* seenTask = nullptr;
        slot.startThen(p.sharedFuture(), InlineExecutor{}, [&](Task& task) noexcept { seenTask = &task; });
        QVERIFY(seenTask == nullptr);

        Task* expected = p.task().get();
        p.setResult(7);
        p.setFinished();
        QCOMPARE(seenTask, expected);  // the finished task is handed to the callback
    }

    void operation_slot_continuation_accepts_nullary() {
        // Like finally(), the startThen() callback may omit the Task& parameter.
        OperationSlot<int> slot;
        Promise<int> p = Promise<int>::create();
        bool ran = false;
        slot.startThen(p.sharedFuture(), InlineExecutor{}, [&]() noexcept { ran = true; });
        QVERIFY(!ran);

        p.setResult(7);
        p.setFinished();
        QVERIFY(ran);
    }

    void operation_slot_stale_continuation_is_guarded() {
        // The core race: between scheduling a completion hook and it firing, the slot is reset and
        // re-assigned to a newer operation. The stale hook must NOT run / must not clobber the new one.
        OperationSlot<int> slot;
        Promise<int> p1 = Promise<int>::create();
        Promise<int> p2 = Promise<int>::create();

        bool firstRan = false, secondRan = false;
        slot.startThen(p1.sharedFuture(), InlineExecutor{}, [&](Task&) noexcept { firstRan = true; });

        // Replace the in-flight operation before the first one finishes.
        slot.reset();
        slot.startThen(p2.sharedFuture(), InlineExecutor{}, [&](Task&) noexcept { secondRan = true; });

        // Completing the FIRST (now stale) operation must not trigger its hook, and must leave the
        // current operation untouched.
        p1.setResult(1);
        p1.setFinished();
        QVERIFY(!firstRan);
        QVERIFY(slot);          // still holding the second operation
        QVERIFY(!secondRan);

        // Completing the current operation runs its hook as normal.
        p2.setResult(2);
        p2.setFinished();
        QVERIFY(secondRan);
    }

    void operation_slot_reset_cancels_underlying_when_sole_owner() {
        // Dropping the slot's future (the sole demand) requests stop on the underlying task.
        OperationSlot<int> slot;
        Promise<int> p = Promise<int>::create();
        slot.startThen(p.sharedFuture(), InlineExecutor{}, [&](Task&) noexcept {});
        QVERIFY(!p.isCanceled());

        // The local promise keeps the task object alive but is not a demanding consumer; resetting the
        // slot drops the last demand and cancels the task.
        slot.reset();
        QVERIFY(p.isCanceled());
    }

    // -----------------------------------------------------------------------
    // Ambient task context (this_task) — the cancellation view Phase 1 builds on
    // -----------------------------------------------------------------------

    void this_task_scope_installs_current_task() {
        QCOMPARE(this_task::get(), static_cast<Task*>(nullptr));
        auto task = std::make_shared<Task>();
        {
            Task::Scope scope(task);
            QCOMPARE(this_task::get(), task.get());
        }
        QCOMPARE(this_task::get(), static_cast<Task*>(nullptr));
        task->setFinished();  // tasks must end finished
    }

    void this_task_throw_if_canceled() {
        auto task = std::make_shared<Task>();
        Task::Scope scope(task);

        QVERIFY(!this_task::isCanceled());
        this_task::throwIfCanceled();  // must not throw

        task->cancel();
        QVERIFY(this_task::isCanceled());
        bool threw = false;
        try { this_task::throwIfCanceled(); }
        catch(const OperationCanceled&) { threw = true; }
        QVERIFY(threw);

        task->setFinished();
    }

    void this_task_cancel_and_throw() {
        auto task = std::make_shared<Task>();
        Task::Scope scope(task);

        bool threw = false;
        try { this_task::cancelAndThrow(); }
        catch(const OperationCanceled&) { threw = true; }
        QVERIFY(threw);
        QVERIFY(task->isCanceled());

        task->setFinished();
    }

    // -----------------------------------------------------------------------
    // Join semantics: the non-blocking quick paths of Task::waitFor()
    // -----------------------------------------------------------------------

    void wait_for_finished_task_returns_true() {
        auto waiting = std::make_shared<Task>();
        Task::Scope scope(waiting);

        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f = p.sharedFuture();
        p.setResult(5);
        p.setFinished();

        // Awaited task already finished -> quick path returns true without blocking.
        const bool ok = Task::waitFor(f.task(), /*throwOnError*/ true,
            /*returnEarlyIfCanceled*/ true, /*cancelWaitingIfAwaitedCanceled*/ true);
        QVERIFY(ok);
        QVERIFY(!waiting->isCanceled());

        waiting->setFinished();
    }

    void wait_for_canceled_task_cancels_waiter() {
        auto waiting = std::make_shared<Task>();
        Task::Scope scope(waiting);

        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f = p.sharedFuture();
        p.cancel();  // awaited reaches the stopped channel

        // returnEarlyIfCanceled + cancelWaitingIfAwaitedCanceled: the waiter is canceled too.
        const bool ok = Task::waitFor(f.task(), /*throwOnError*/ true,
            /*returnEarlyIfCanceled*/ true, /*cancelWaitingIfAwaitedCanceled*/ true);
        QVERIFY(!ok);
        QVERIFY(waiting->isCanceled());

        p.reset();
        waiting->setFinished();
    }

    // -----------------------------------------------------------------------
    // StopToken / StopSource / StopCallback facade (Phase 1)
    // -----------------------------------------------------------------------

    void stop_token_empty() {
        StopToken token;
        QVERIFY(!static_cast<bool>(token));
        QVERIFY(!token.stop_requested());
        QVERIFY(!token.stop_possible());
    }

    void stop_source_request_stop() {
        StopSource src;
        StopToken token = src.get_token();

        QVERIFY(static_cast<bool>(token));
        QVERIFY(token.stop_possible());
        QVERIFY(!token.stop_requested());

        QVERIFY(src.request_stop());        // first request performs the stop
        QVERIFY(token.stop_requested());
        QVERIFY(src.stop_requested());
        QVERIFY(token.stop_possible());     // already-requested implies still possible

        QVERIFY(!src.request_stop());       // second request is a no-op
    }

    void stop_token_reflects_task_cancellation() {
        Promise<int> p = Promise<int>::create();
        StopToken token(p.task());

        QVERIFY(!token.stop_requested());
        p.cancel();
        QVERIFY(token.stop_requested());

        p.reset();  // drive to finished
    }

    void stop_token_equality_and_swap() {
        StopSource src;
        StopToken a = src.get_token();
        StopToken b = src.get_token();
        StopToken empty;

        QVERIFY(a == b);        // same stop state
        QVERIFY(!(a == empty));

        a.swap(empty);
        QVERIFY(!static_cast<bool>(a));
        QVERIFY(empty == b);
    }

    void stop_callback_fires_on_cancel() {
        StopSource src;
        int fired = 0;
        {
            StopCallback cb(src.get_token(), [&]() noexcept { fired++; });
            QCOMPARE(fired, 0);  // not yet requested
            src.request_stop();
            QCOMPARE(fired, 1);  // fired exactly once on cancellation
        }
    }

    void stop_callback_fires_immediately_if_already_requested() {
        StopSource src;
        src.request_stop();

        int fired = 0;
        StopCallback cb(src.get_token(), [&]() noexcept { fired++; });
        QCOMPARE(fired, 1);  // fires in the constructing thread when already requested
    }

    void stop_callback_silent_on_value_completion() {
        Promise<int> p = Promise<int>::create();
        int fired = 0;
        StopCallback cb(StopToken(p.task()), [&]() noexcept { fired++; });

        p.setResult(1);
        p.setFinished();  // value channel, not stopped
        QCOMPARE(fired, 0);
    }

    void this_task_get_stop_token() {
        QVERIFY(!static_cast<bool>(this_task::get_stop_token()));  // no active task -> empty token

        auto task = std::make_shared<Task>();
        {
            Task::Scope scope(task);
            StopToken token = this_task::get_stop_token();
            QVERIFY(static_cast<bool>(token));
            QVERIFY(!token.stop_requested());

            task->cancel();
            QVERIFY(token.stop_requested());
            QVERIFY(this_task::isCanceled());  // raw helper agrees with the token view
        }

        QVERIFY(!static_cast<bool>(this_task::get_stop_token()));  // scope exited
        task->setFinished();
    }

    // -----------------------------------------------------------------------
    // TaskScope — the owned-scope / nursery tier (Phase 2)
    // -----------------------------------------------------------------------

    void taskscope_installs_ambient_task() {
        QCOMPARE(this_task::get(), static_cast<Task*>(nullptr));
        {
            TaskScope scope(TaskScope::Isolated);
            QVERIFY(this_task::get() != nullptr);
            QCOMPARE(this_task::get(), scope.task().get());
            QVERIFY(!scope.stop_requested());
        }
        QCOMPARE(this_task::get(), static_cast<Task*>(nullptr));  // ambient restored on scope exit
    }

    void taskscope_request_stop() {
        TaskScope scope(TaskScope::Isolated);
        StopToken token = scope.get_stop_token();
        QVERIFY(!token.stop_requested());

        scope.request_stop();
        QVERIFY(scope.stop_requested());
        QVERIFY(token.stop_requested());
    }

    void taskscope_bound_parent_cancel_propagates_down() {
        auto parent = std::make_shared<Task>();
        Task::Scope parentScope(parent);
        {
            TaskScope scope(TaskScope::Bound);  // becomes a child of 'parent'
            QVERIFY(!scope.stop_requested());

            parent->cancel();  // downward propagation: canceling the parent cancels the scope
            QVERIFY(scope.stop_requested());
        }
        parent->setFinished();
    }

    void taskscope_bound_scope_cancel_propagates_up() {
        auto parent = std::make_shared<Task>();
        Task::Scope parentScope(parent);
        {
            TaskScope scope(TaskScope::Bound);
            scope.request_stop();  // upward propagation: canceling the scope cancels the parent
            // (the upward link fires when the scope task finishes, i.e. at scope destruction)
        }
        QVERIFY(parent->isCanceled());
        parent->setFinished();
    }

    void taskscope_spawn_inline_runs_and_joins() {
        TaskScope scope(TaskScope::Isolated);
        int sideEffect = 0;
        Future<int> f = scope.spawn(InlineExecutor{}, [&]() { sideEffect = 99; return 7; });

        QCOMPARE(sideEffect, 99);  // the InlineExecutor ran the child synchronously
        QVERIFY(f.isFinished());
        QCOMPARE(f.result(), 7);
        // Scope destruction cancels-and-joins the (already finished) child without blocking.
    }

    void taskscope_adopt_finished_child() {
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> f = p.sharedFuture();
        p.setResult(3);
        p.setFinished();
        {
            TaskScope scope(TaskScope::Isolated);
            scope.adopt(f);
            // Scope exit cancels-and-joins the adopted child; it is already finished, so this is a no-op.
        }
        QCOMPARE(f.result(), 3);  // adoption did not disturb the result
    }

    // -----------------------------------------------------------------------
    // SharedAsyncValue — the shared / memoized / demand-stopped tier (Phase 3)
    // -----------------------------------------------------------------------

    void shared_async_value_empty() {
        SharedAsyncValue<int> slot;
        QVERIFY(slot.expired());
        QVERIFY(!static_cast<bool>(slot.attach()));
    }

    void shared_async_value_attach_live_producer() {
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> producer = p.sharedFuture();  // a consumer holds the demand
        SharedAsyncValue<int> slot = producer;          // the registry memoizes it weakly

        QVERIFY(!slot.expired());
        SharedFuture<int> attached = slot.attach();
        QVERIFY(static_cast<bool>(attached));  // a live, non-canceled producer is handed out

        p.setResult(5);
        p.setFinished();
        QCOMPARE(attached.result(), 5);  // fan-out: the attached consumer sees the result
    }

    void shared_async_value_expires_when_demand_gone() {
        SharedAsyncValue<int> slot;
        {
            Promise<int> p = Promise<int>::create();
            SharedFuture<int> producer = p.sharedFuture();
            slot = producer;
            QVERIFY(!slot.expired());
            p.setResult(1);
            p.setFinished();
        }  // the only consumer (and the promise) are gone -> the weak slot expires
        QVERIFY(slot.expired());
        QVERIFY(!static_cast<bool>(slot.attach()));
    }

    void shared_async_value_attach_rejects_canceled_producer() {
        // The race-safe-attach case: the weak entry has NOT expired (a consumer still holds the task),
        // but the producer has already been canceled (mid-stop). attach() must report it unavailable so
        // the caller relaunches instead of handing back a value that is being torn down.
        Promise<int> p = Promise<int>::create();
        SharedFuture<int> producer = p.sharedFuture();
        SharedAsyncValue<int> slot = producer;

        p.cancel();  // request stop, but 'producer' still keeps the task object alive (not expired)
        QVERIFY(!slot.expired());
        QVERIFY(!static_cast<bool>(slot.attach()));  // canceled producer is rejected

        p.reset();  // drive to finished
    }

    void shared_async_value_get_or_start() {
        int launches = 0;
        auto launcher = [&]() -> SharedFuture<int> {
            launches++;
            return SharedFuture<int>(Future<int>::createImmediate(42));
        };

        SharedAsyncValue<int> slot;
        SharedFuture<int> a = slot.get_or_start(launcher);
        QCOMPARE(launches, 1);       // started a fresh producer
        QCOMPARE(a.result(), 42);

        // 'a' still demands the producer, so a second request attaches to the same one (no relaunch).
        SharedFuture<int> b = slot.get_or_start(launcher);
        QCOMPARE(launches, 1);
        QCOMPARE(b.result(), 42);
    }

    // -----------------------------------------------------------------------
    // Phase 4: ScopedFuture as the scope-aware coroutine return type
    // (the per-await SCFuture tag has been retired; the awaiting coroutine's
    // own scope, not the awaited handle's type, now drives join-vs-detach)
    // -----------------------------------------------------------------------

    void scoped_future_is_an_ordinary_future() {
        // ScopedFuture is solely the scope-aware coroutine return type; to its consumer it is an
        // ordinary Future<R> (no extra state), selecting the structured-concurrency promise type.
        static_assert(std::is_base_of_v<Future<int>, ScopedFuture<int>>);
        static_assert(std::is_same_v<ScopedFuture<int>::promise_type, CoroutinePromise<int, true>>);
        QVERIFY(true);
    }

    void scoped_coroutine_returns_value() {
        // A scope-aware coroutine delivers its value through the value channel like any future.
        ScopedFuture<int> f = scopedCoroReturn(21);
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());
        QCOMPARE(f.result(), 42);
    }

    void scoped_coroutine_awaits_owned_value() {
        // Awaiting a finished (owned) Future inside a scope-aware coroutine resumes inline.
        ScopedFuture<int> f = scopedCoroAwaitValue(Future<int>::createImmediate(41));
        QVERIFY(f.isFinished());
        QCOMPARE(f.result(), 42);
    }

    void scoped_coroutine_awaits_shared_value() {
        // Awaiting a Tier-2 shared value (SharedFuture) is supported too; the consumer sees a plain result.
        ScopedFuture<int> f = scopedCoroAwaitShared(SharedFuture<int>(Future<int>::createImmediate(41)));
        QVERIFY(f.isFinished());
        QCOMPARE(f.result(), 42);
    }

    void scoped_coroutine_propagates_error() {
        // The error channel of an awaited future surfaces as the coroutine's error channel
        // (rethrown by await_resume, captured by the promise's unhandled_exception).
        ScopedFuture<int> f = scopedCoroAwaitValue(Future<int>::createFailed(Exception(QStringLiteral("boom"))));
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());  // an error is not a cancellation
        QVERIFY(f.getExceptionIfFailed().has_value());
    }

    // -----------------------------------------------------------------------
    // The keep-alive rule (ARCHITECTURE.md §6): a coroutine member function of
    // an OvitoObject automatically keeps 'this' alive for its whole lifetime,
    // via the self-guard installed by CoroutinePromiseBase's constructor.
    // -----------------------------------------------------------------------

    void coroutine_member_keeps_this_alive_across_suspension() {
        // A pending input the coroutine will await (kept unfinished so the coroutine suspends).
        Promise<int> input = Promise<int>::create();

        // Create the object and remember a weak reference so we can detect its destruction.
        OORef<SelfGuardTestObject> obj = OORef<SelfGuardTestObject>::create();
        std::weak_ptr<OvitoObject> weak = obj;

        // Start the member coroutine. It runs eagerly up to 'co_await input' and suspends there.
        Future<int> result = obj->addMember(input.future());
        QVERIFY(static_cast<bool>(result));
        QVERIFY(!result.isFinished());

        // Drop the ONLY external strong reference to the object while the coroutine is still suspended.
        obj.reset();

        // Without the automatic self-guard this would have destroyed the object. It must still be alive,
        // because the suspended coroutine's promise holds a strong reference to it.
        QVERIFY(!weak.expired());

        // Complete the awaited input. This resumes the coroutine inline; it reads 'memberValue' (only
        // valid because the object is still alive) and then runs to completion.
        input.setResult(30);
        input.setFinished();

        QVERIFY(result.isFinished());
        QVERIFY(!result.isCanceled());
        QCOMPARE(std::move(result).result(), 30 + 12345);

        // The coroutine has finished, so its frame (and the self-guard inside it) is gone and the object
        // is finally released.
        QVERIFY(weak.expired());
    }

    void coroutine_member_self_guard_released_on_cancellation() {
        // Variant of the above for the stopped channel: if the coroutine is canceled while suspended
        // (e.g. demand drops to zero), unwinding its frame must release the self-guard so the object is
        // not leaked.
        Promise<int> input = Promise<int>::create();
        OORef<SelfGuardTestObject> obj = OORef<SelfGuardTestObject>::create();
        std::weak_ptr<OvitoObject> weak = obj;

        Future<int> result = obj->addMember(input.future());
        obj.reset();
        QVERIFY(!weak.expired());  // kept alive by the suspended coroutine

        // Drop the future: demand reaches zero, cancelling the coroutine, which unwinds and releases its
        // self-guard. (Dropping the input promise as well drives the chain to a finished state.)
        result.reset();
        input.reset();
        QVERIFY(weak.expired());
    }
};

QTEST_MAIN(ConcurrentTest)
#include "tst_concurrent.moc"
