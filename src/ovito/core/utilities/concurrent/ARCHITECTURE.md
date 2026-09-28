# OVITO's Asynchronous Task Framework — Design Reference

This document is the shared reference for OVITO's homegrown asynchronous task framework
(`ovito/src/ovito/core/utilities/concurrent/`). It gives the primitives (`Task`, `Future`,
`SharedFuture`, `Promise`, executors, `TaskDependency`, `this_task`) one consistent vocabulary and
records the structured-concurrency design the framework arrived at.

OVITO does **not** use `std::execution` and does not vendor `stdexec`. The model is, however,
deliberately shaped so that its concepts line up with `std::execution` (value/error/stopped
completion channels, schedulers, stop tokens, `split`/`ensure_started`, nurseries), so that a future
alignment — should we ever want it — would be a small jump rather than a rewrite.

The observable behavior described here is pinned by the QTest suite in
`tests/cpp/core/utilities/concurrent/`.

---

## 1. The async operation and its three completion channels

`Task` is the shared state of one asynchronous operation. It is reference-counted
(`std::enable_shared_from_this`) and shared between the producing `Promise` and the consuming
`Future`/`SharedFuture`.

A task always ends in the **finished** state (`Task::Finished`). Layered on top of "finished" are
three mutually exclusive *completion channels*. This vocabulary mirrors `std::execution` and is a
*naming convention* over the underlying state API:

| Completion channel | State of the `Task`                             | `std::execution` analogue |
|--------------------|-------------------------------------------------|---------------------------|
| **value**          | `Finished`, result stored, no exception         | `set_value`               |
| **error**          | `Finished`, `_exceptionStore` set               | `set_error`               |
| **stopped**        | `Canceled` (and eventually `Finished`)          | `set_stopped`             |

- **error ≠ stopped.** A task that stored an exception is *failed* (error channel); a task that was
  canceled is *stopped*. The distinction is real in the code — `getExceptionIfFailed()` returns
  `std::nullopt` for a canceled task — and the channel vocabulary names it.
- **`Canceled` does not by itself imply `Finished`.** `Task::cancel()` sets `Canceled` but not
  `Finished`; the task reaches `Finished` later (e.g. via `cancelAndFinish()` when the `Promise` is
  dropped, or when the worker observes cancellation and unwinds). A consumer can therefore observe
  `isCanceled() == true` while `isFinished() == false`.
- **Reading the channels:** `isFinished()`, `isCanceled()`, `exceptionStore()` /
  `getExceptionIfFailed()`, and `result()` are the channel accessors. `result()` is valid only on a
  finished, non-canceled task and rethrows a stored exception (so it surfaces the error channel as a
  C++ exception).

---

## 2. Executors are schedulers

An **executor** answers the question *where and when does this continuation run?* — it is OVITO's
spelling of a `std::execution` **scheduler**. `executor.execute(f)` is "schedule `f`."

The implementations available:

| Executor                      | Runs the work…                                                       |
|-------------------------------|----------------------------------------------------------------------|
| `InlineExecutor`              | immediately, in place, on the calling thread                         |
| `ThreadPoolExecutor`          | on a worker thread of the `TaskManager` pool (optionally high-prio)  |
| `ObjectExecutor`              | immediately, in place, when called in the main thread; otherwise, deferred onto the main thread, bound to an `OvitoObject`'s lifetime |
| `DeferredObjectExecutor`      | deferred onto the main-thread work queue, bound to an object         |
| `QObjectExecutor`             | in the thread affinity of a `QObject`                                |

`ObjectExecutor` / `DeferredObjectExecutor` / `QObjectExecutor` additionally encode an **object-lifetime → stopped**
link (via a weak object reference): if the bound object dies, the scheduled work is dropped. The
scope model in §4 composes with this link rather than duplicating it.

**One pool, priority is a task property.** `ThreadPoolExecutor` submits to a single `QThreadPool`
(`TaskManager::startWork()`). Work inherits the `HighPriority` flag of the scheduling task (set only
by the interactive viewport render, `ViewportWindow`). High-priority work (a) *oversubscribes* the
pool — `startWork()` raises the pool's thread limit by one (`beginOversubscription()`) so the task
starts immediately on a fresh worker even when every regular thread is busy with long low-priority
work; the limit is restored (`endOversubscription()`) once the high-priority worker finishes — and
(b) runs its worker thread at normal OS priority (`WorkerThreadPriorityScope`), while regular work
runs the pool's threads at low priority. The single-pool model makes priority follow the task rather
than the pool it lands in, and bounds extra threads to the (small) number of concurrent
high-priority tasks. (Raising the limit is used rather than `reserveThread()` /
`startOnReservedThread()`, which only adjust QThreadPool's reservation bookkeeping and do not
reliably spawn a worker on a long-lived, saturated pool — see `tst_concurrent_pool`.) `parallelFor`
keeps using plain `start()` plus master-thread execution of un-started workers — it is
self-balancing and bounded, and its worker count can far exceed the core count, so it is
deliberately *not* oversubscribed.

---

## 3. Cancellation is one unified stop concept

OVITO has exactly **one** canceled-state of record: `Task::Canceled`, set by `Task::cancel()`.
Everything else is a *source* that requests that stop, or a *view* that reads it. There is no second
cancellation system.

Stop **sources**:

- **Demand-counted stop.** `detail::TaskDependency` increments a task's `_dependentsCount` on
  construction and decrements on destruction; when the count reaches zero the task is canceled
  (`~TaskDependency`). In words: *a task is kept alive only as long as someone demands its result;
  stop is requested when demand drops to zero.* This is how dropping the last `Future` cancels its
  task.
- **Explicit cancellation.** `Promise::reset()` / `~Promise` (`cancelAndFinish()`), and a parent
  task propagating cancellation to children (e.g. `MainThreadOperation`).

Stop **views / ambient context**:

- **`this_task::get()`** is the thread-local "current task" — the implicit operation a worker runs
  under. `Task::Scope` installs it for a region.
- **`this_task::isCanceled()` / `throwIfCanceled()` / `cancelAndThrow()`** read (and, for the
  latter, set) the current task's stopped channel. Cooperative cancellation: long-running work polls
  `throwIfCanceled()` and unwinds via the `OperationCanceled` exception.

**`StopToken` / `StopSource` / `StopCallback` facade** (`StopToken.h`). A cheap, copyable
`StopToken` (`stop_requested()` reads `Task::Canceled`; `stop_possible()`), a move-only `StopSource`
(`request_stop()` calls `Task::cancel()`; owns and finishes a fresh `Task`), and a `StopCallback`
(fires once on cancellation; registration reuses `detail::TaskCallback` / `FunctionTaskCallback`).
These are shaped deliberately like `std::stop_token` / `std::stop_source` / `std::stop_callback` but
backed by `Task`, so there remains exactly one canceled-state of record. `this_task::get_stop_token()`
exposes the current task's stopped channel as a token; the hot polling helpers
(`isCanceled()` / `throwIfCanceled()`) stay raw-pointer reads of the same `Task::Canceled` bit to
avoid per-poll `shared_ptr` traffic, and are the cooperative-stop view of the token. The facade gives
the *view* a first-class, named type; the demand-counted and explicit sources are *re-described* as
stop sources, not replaced.

---

## 4. Two tiers of structured concurrency

OVITO needs **two** structured-concurrency tiers, not one. The same intuitive idea — "a task's
lifetime is bounded by who needs it" — is realized two different ways depending on whether the task
has a single owner or many consumers.

### Tier 1 — Owned scope (the tree / nursery)

A scope that **owns** a set of child tasks and, on exit, **requests stop and joins** — waits for the
children to actually reach a completion channel before the scope returns. Children are strictly
nested in the scope's lifetime; the scope's stop request propagates downward.

`TaskScope` (`TaskScope.h`) is the owned-scope ("nursery") type. It owns a scope task (built on the
shared `detail::ScopeTask` parent-link, also used by `MainThreadOperation`), installs it as the
ambient `this_task::get()`, exposes a `StopToken`, and adopts children via `spawn()` / `adopt()`.
Contract: **on destruction it requests stop on every child it owns and joins them** (waits for each
to complete) before finishing its own task — so no adopted child outlives the scope still running.
Downward stop propagation to children reuses the `StopCallback` facade. `MainThreadOperation` is
re-expressed on the same `detail::ScopeTask`. The `parallelCancellable` latch join in `ParallelFor.h`
is a hand-rolled specialization of the same Tier-1 pattern (a perf-critical loop with master-thread
work stealing).

### Tier 2 — Shared async value (the DAG boundary)

An eagerly-started, **memoized** result that multiple consumers in *different* scopes attach to —
the pipeline-evaluation pattern. Because it has many independent "parents," it cannot live inside a
single Tier-1 tree; it is the structured-concurrency *escape hatch*, the analogue of
`std::execution`'s `split` / `ensure_started`. Its three defining properties:

- **Demand-counted lifetime/stop:** alive ⇔ ≥1 consumer demands it; stopped when the last leaves
  (the `_dependentsCount` mechanism from §3, acting here as a *merged* stop source: "stop only when
  all consumers have stopped").
- **Weak registry, not owner:** the dedup table (`PipelineCache`) holds a `WeakSharedFuture`, so it
  provides *discovery* (attach to an in-flight computation) without *ownership*. A strong reference
  here would leak unstructured background work.
- **Fan-out value/error:** completion replays to all current consumers (`SharedFuture`).

`SharedAsyncValue<R>` (`SharedAsyncValue.h`) names this pattern (`SharedFuture` + `WeakSharedFuture`
+ demand count) as the registry slot for a shared, memoized, demand-stopped producer. Its `attach()`
is the **race-safe** lock: `WeakSharedFuture::lock()` can succeed on a producer whose task is already
canceled (last consumer dropped demand, requesting stop, but the weak entry has not yet expired), so
`attach()` reports such a producer as *unavailable* and the caller relaunches a fresh one.
`get_or_start()` packages the attach-or-relaunch dance for self-contained callers. `PipelineCache`
stores a `SharedAsyncValue<PipelineFlowState>` and uses `attach()`, centralizing the previously
inline `lock()` + `!isCanceled()` check.

### Owner-held single slot (the in-flight-operation member)

Many objects keep "the one operation I am currently running" as a member: `FileSource`'s frame scan,
`PipelineCache`'s trajectory precomputation, `ScenePreparation`'s pipeline evaluation. This is a
**strong-owning Tier-1** slot — the owner drives the work and wants it to keep running even if every
external consumer drops its future — and is the structural counterpart to the weak, demand-stopped
Tier-2 `SharedAsyncValue` (strong vs. weak; owner-driven vs. consumer-driven). It may still fan the
result out to consumers, and on completion it **self-manages**: it either clears the slot or advances
to the next step.

The subtle, repeatedly hand-rolled part is the **identity-guarded completion hook**. The hook is
scheduled through an owner-bound executor (`ObjectExecutor` / `DeferredObjectExecutor` / `QObjectExecutor`),
so between scheduling it and it firing an external invalidation path may have reset the slot and
re-assigned it to a *newer* operation; a naive reset/advance in the hook would then clobber that newer
operation. `OperationSlot<R>` (`OperationSlot.h`) encapsulates this: it fires the hook only if the slot
still refers to the very task that just finished (`future().task().get() == &finishedTask`).
`startAutoReset()` clears the slot on completion; `startThen()` runs a continuation that typically
advances the slot. The owner-bound executor must outlive-guard the hook (dropped if the owner dies).
`FutureWatcher` shares the same identity-guarded pattern but is deliberately **not** built on
`OperationSlot`: it must additionally keep the finished task alive across Qt signal emission (so a
`result()` call from one signal handler still works after another handler reset the future), which the
single consumer-facing future of `OperationSlot` cannot express. `decodeOutcome()`
(`detail/TaskOutcome.h`) is the shared free helper that classifies a finished task into its
value/error/stopped channel (cancellation dominates a stored exception).

### The reconciling rule: join vs. detach

A consumer that lives in a Tier-1 owned scope decides per awaited child whether to join or detach on
cancellation. The default is **detach** — it is OVITO's pre-existing contract (most coroutines are
self-contained and own their inputs, §6), and the safe-on-omission direction is the *responsiveness*
cost of an unnecessary detach, not a use-after-free. **Join is the opt-in**, declared by the child:

> **`ScopedFuture` child ⇒ join** (the child opted into structured ownership; the scope waits for it).
> **plain `Future` child ⇒ detach** (self-contained by default; drop the dependency, O(1), non-blocking).
> **shared value (`SharedFuture`) ⇒ detach** (a Tier-2 producer governed by its *demand count*, never
> joined by a single awaiter).

Detaching is exactly what `~TaskDependency` does. A coroutine declares its own join requirement through
its return type: returning a `ScopedFuture<R>` says "I borrow from my parent / must be joined"; a plain
`Future<R>` says "I am self-contained, safe to drop." The child's author is the one who knows whether it
holds borrowed state across a suspension point, so the declaration lives with the child — and the
decision to join is read off the child alone.

**Realized in the coroutine awaiter.** The join-vs-detach decision is made *per await edge* from the
**awaited child's declared type alone**: in `FutureAwaiter::await_suspend` the keep-alive flag is
`is_scoped_future_v<awaited>`. When the awaiting coroutine is canceled exactly as it suspends on the
child, a `ScopedFuture` child keeps the awaiting frame alive until the child finishes (so the child's
borrow into that frame stays valid); a plain `Future` or shared `SharedFuture` child is dropped
immediately. The mechanism is the `if constexpr` keep-alive branch in `TaskAwaiter::whenTaskFinishes`,
now driven by the child's type.

**The two edges are independent — no propagate-up.** The awaiting coroutine's *own* return type (`SC`)
does **not** enter the decision above. `SC` is a declaration about a *different* edge — whether the
awaiting coroutine `P` borrows from *its* parent — and conflating the two would be wrong. A `ScopedFuture`
child `C` being scoped means *`C` borrows from `P`'s frame*, which the `P`→`C` join above already handles
using `C`'s type and keeps `P`'s frame alive regardless of `P`'s return type. Whether `P` must in turn be
joined by *its* caller is the separate question of whether *`P`* borrows from its caller's frame — which
only `P`'s author can answer, and which they declare through `P`'s return type. So a self-contained
producer (a Tier-2 pipeline producer fed to a `FutureCache`, or `AmbientOcclusionModifier`'s occlusion
pass, which owns all its inputs yet lends a render target to a scoped per-frame render) correctly returns
a plain `Future` while still joining its scoped children. The chain of `ScopedFuture` returns therefore
extends upward only as far as borrowed state actually reaches, terminating naturally at the first
coroutine that owns everything it touches — or, where a `ScopedFuture` must be presented to non-coroutine
code, at a forwarder that slices `ScopedFuture`→`Future`, or at the synchronous **join root** that drives
the task to completion by pumping the event loop (`ProgressDialog::showForFuture`) or blocking
(`waitForFinished`/`blockForFuture`). `ScopedFutureAwaiter` is the opt-in, self-documenting awaiter that
additionally requires the awaited handle to be a `ScopedFuture`, making a nested structured link explicit
at the call site.

**`then` is the unstructured edge.** `then()`/`postprocess()` form detached continuations, not `co_await`
join edges, so they always yield a single-consumer `Future<R>` (never a `ScopedFuture`, and never a
`SharedFuture` even when invoked on one); the continuation's returned future is unwrapped to its value
type. `then()`/`postprocess()` are therefore `= delete`d on `ScopedFuture`: consume a scope with
`co_await`, or lower it to a plain `Future` explicitly before attaching a continuation.

The sequential-loop and await-all combinators (`for_each_sequential`, `reduce_sequential`, `whenAll`)
have an **adaptive return type** — they return a `ScopedFuture` when their per-iteration/element
sub-tasks are themselves `ScopedFutures`, and a plain `Future` otherwise. A combinator is a *transparent
forwarder*: it joins its scoped sub-tasks per the child-gated rule, but those sub-tasks typically borrow
from the combinator's *caller* (the values being awaited were created there), so the combinator adopts a
scoped return type to forward that join obligation one level up — it is the one place where a node's
return type tracks its children rather than its own captures, precisely because the combinator stands in
for its caller. `PythonInterface::executeAsync` returns a plain `Future`: it forms no owned scope (it
only awaits the deferred executor, never a child future), so its callers detach from it.

---

## 5. Design notes and known limitations

- **Cancellation requests stop; it does not join.** Dropping a `Future` *requests stop* but does
  **not** wait for the task to actually stop — work can still be running after the consumer has moved
  on. The structural fix is to use a Tier-1 owned scope (`TaskScope`, join-on-exit); the `syncWait()`
  helper covers the narrower case where a worker captures locals by reference and must not outlive
  them.
- **Scope membership is opt-in.** The *global* launch parent-link rule is not unified with
  `TaskScope`: `launchTask` / `asyncLaunch` do not automatically attach a new task to the current
  scope. The prerequisite — the owned-vs-shared (join-vs-detach) distinction — exists (§4), but the
  rewire is invasive (≈66 launch sites, including the pipeline evaluators) and must keep shared
  producers on the detach path, so attaching a task to a scope is done explicitly via
  `TaskScope::spawn` / `adopt`.
- **Worker-thread `waitFor()` releases its pool slot.** A pool thread that blocks in
  `Task::waitFor()` releases its slot for the duration of the wait
  (`QThreadPool::releaseThread()`, restored with `reserveThread()` on wake), letting the pool exceed
  its maximum and start another worker. This prevents a saturated-pool deadlock where every pool
  thread blocks waiting on a task that itself needs a pool thread to run. The main-thread path avoids
  the problem by pumping the event loop instead. One caveat remains: if the waiting task has *already*
  been canceled when `waitFor()` is entered, the wait returns early rather than joining.

---

## 6. The keep-alive rule: detached work owns its inputs

§4 gives two ways to bound a task's lifetime by who needs it. There is a third, more local obligation
that applies whenever a task is **detached** rather than owned-and-joined — i.e. the launching scope
does *not* wait for it (a `then()` continuation, an `asyncLaunch` worker, a Tier-2 shared producer, or
any `Future` whose consumer simply drops it). Because such a task may still be running after the code
that launched it has unwound, it cannot *borrow* anything from that code:

> **A detached asynchronous task must *own* (hold a strong reference to) everything it touches** — its
> `this` object and every input — for the whole time it runs. It must never capture a bare `&`/raw
> pointer into a frame that can disappear first.

This is the counterpart to the join-vs-detach rule of §4.3: an **owned child is joined**, so borrowing
from the parent is safe; a **detached task is not joined**, so it must own instead. (The alternative —
making the launcher join — is the Tier-1 owned scope; `syncWait()` is the narrow blocking form for a
worker that borrows stack locals.) Owning is preferable here when you want non-blocking teardown and
fast cooperative cancellation: the launcher unwinds immediately and the orphaned task keeps its inputs
alive until it observes stop and finishes.

A closure has **two** distinct keep-alive obligations, and both are easy to forget:

1. **Inputs.** Capture data objects, a `FrameGraph`, etc. by owning references (`OORef` / `DataOORef`),
   never by `&`. Prefer APIs that hand out *owning* parameters so the safe capture is the natural one —
   e.g. `ViewportOverlay::render` / `DataVis::renderAsynchronous` receive `OORef<FrameGraph>` (not `FrameGraph&`),
   so the owning capture is what's already in scope.
2. **`this`.** A task implemented as a *member function* implicitly borrows `this`. Make the keep-alive
   structural rather than manual:
   - **Coroutine member functions** keep `this` alive **automatically**: `CoroutinePromiseBase` captures
     a strong reference to the object (the implicit first argument the compiler passes to the promise
     constructor) for the whole coroutine lifetime. The old manual `OORef<Self> self(this)` self-guard
     at the top of such coroutines is therefore unnecessary.
   - **Main-thread continuations** scheduled with `ObjectExecutor(this)` / `DeferredObjectExecutor(this)`
     are *dropped* if the object dies before they start (object-lifetime → stopped, §2) — sufficient for
     short continuations, but it guards the *start*, not the *duration*.
   - **Worker-thread member tasks** (`asyncLaunch`) outlive any scheduling guard, so they must hold a
     strong self-reference for their duration: use `asyncLaunch(this, [](Self& self){ … })` (`Launch.h`),
     which bundles the keep-alive into the launch primitive and hands the body a live reference.

---

## 7. Map: concept → code

| Concept (this document)             | Where it lives                                                    |
|-------------------------------------|------------------------------------------------------------------|
| async operation / shared state      | `Task`, `TaskWithStorage`                                         |
| value / error / stopped channels    | `Task` state bits, `_exceptionStore`, `result()`                 |
| producer handle                     | `Promise` / `PromiseBase`                                         |
| consumer handle (single / shared)   | `Future` / `SharedFuture` (`FutureBase`)                          |
| scheduler                           | `*Executor` (`execute()`)                                         |
| continuation                        | `then()`, `finally()`, `detail::ContinuationTask`, `TaskAwaiter`  |
| demand-counted stop source          | `detail::TaskDependency` (`_dependentsCount`)                     |
| ambient stop context                | `this_task::get()`, `Task::Scope`                                 |
| cooperative stop view               | `this_task::isCanceled/throwIfCanceled/cancelAndThrow`            |
| stop token / source / callback      | `StopToken`, `StopSource`, `StopCallback`, `this_task::get_stop_token()` |
| Tier-1 owned scope                  | `TaskScope` (`MainThreadOperation` and `parallelCancellable` latch are specializations) |
| Tier-2 shared value                 | `SharedAsyncValue` (`SharedFuture` + `WeakSharedFuture` + demand count; used by `PipelineCache`, `FileManager`, and the trajectory/averaging modifiers) |
| Tier-1 owner-held single slot       | `OperationSlot` (`OperationSlot.h`) — strong-owning, identity-guarded self-reset/advance; used by `FileSource`, `PipelineCache`, `ScenePreparation` |
| finished-task outcome classifier    | `detail::decodeOutcome` (`detail/TaskOutcome.h`) — value/error/stopped channel; cancel dominates exception (used by `FutureWatcher`) |
| owned-scope coroutine return type   | `ScopedFuture<R>` (selects `CoroutinePromise<R, true>`; "I borrow from my parent / join me" marker. `FutureAwaiter` joins a child iff `is_scoped_future_v<awaited>` — the child's type alone, independent of the awaiting coroutine's own return type; `ScopedFutureAwaiter` is the opt-in form requiring a scoped child) |
| sequential async loop               | `for_each_sequential` / `reduce_sequential` (`ForEach.h`, `Reduce.h`) — adaptive return type: `ScopedFuture<R>` when the sub-tasks are scoped, else `Future<R>` |
| await-all combinator                | `whenAll` (`WhenAll.h`) — a coroutine that awaits each future in a range without consuming it (`detail::RewindingFutureAwaiter`) |
| callback plumbing                   | `detail::TaskCallbackBase`, `FunctionTaskCallback`               |
| join primitive                      | `Task::waitFor`                                                   |
| keep-alive rule (§6)                | detached task owns its inputs + `this`                           |
| automatic `this` keep-alive         | `CoroutinePromiseBase` self-guard constructor (coroutine members) |
| explicit input keep-alive           | `asyncLaunch(this, fn)` (`Launch.h`); `OORef<FrameGraph>` render params |
