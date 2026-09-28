// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "SharedFuture.h"

namespace Ovito {

/**
 * \brief Holds the single asynchronous operation an object currently has in flight, and supervises
 *        its completion — the strong-owning "Tier-1" single-slot counterpart to the weak,
 *        demand-stopped Tier-2 SharedAsyncValue (see ARCHITECTURE.md §4).
 *
 * Many OVITO objects keep "the one operation I am currently running" as a member: FileSource's frame
 * scan, PipelineCache's trajectory precomputation, ScenePreparation's pipeline evaluation. Such a slot
 * is **strong-owning** (the owner drives the work and wants it to keep running even if every external
 * consumer drops its future), may **fan out** the result to multiple consumers (via future()), and
 * **self-manages** on completion: it either clears itself or advances to the next step.
 *
 * The subtle, repeatedly hand-rolled part this type encapsulates is the **identity-guarded completion
 * hook**. The hook is scheduled through an owner-bound executor (ObjectExecutor / DeferredObjectExecutor
 * / QObjectExecutor), so between scheduling it and it firing, an external invalidation path may have
 * reset() the slot and re-assigned it to a *newer* operation. A naive reset/advance in the hook would
 * then clobber that newer operation. OperationSlot fires the hook only if the slot still refers to the
 * very task that just finished (`future().task().get() == &finishedTask`).
 *
 * The executor passed to the start*() functions must be bound to the owner's lifetime (so the hook is
 * dropped if the owner dies while the operation is still in flight) — exactly what ObjectExecutor,
 * DeferredObjectExecutor and QObjectExecutor provide. The hook is invoked noexcept: a callback whose
 * logic may throw must handle its own exceptions (see PipelineCache::precomputeNextAnimationFrame()).
 *
 * The slot captures `this` in its completion hook and is therefore neither copyable nor movable; it is
 * meant to live as a data member of the long-lived owner object that drives it.
 */
template<typename R>
class OperationSlot
{
public:

    /// Constructs an empty slot, with no operation in flight.
    OperationSlot() noexcept = default;

    /// The slot installs a `this`-capturing completion hook, so it must stay put.
    OperationSlot(const OperationSlot&) = delete;
    OperationSlot& operator=(const OperationSlot&) = delete;
    OperationSlot(OperationSlot&&) = delete;
    OperationSlot& operator=(OperationSlot&&) = delete;

    /// Returns whether an operation is currently in flight (the slot is occupied).
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(_future); }

    /// Returns whether an operation is currently in flight (the slot is occupied).
    [[nodiscard]] bool active() const noexcept { return static_cast<bool>(_future); }

    /// Returns whether the in-flight operation has reached the finished state.
    [[nodiscard]] bool isFinished() const noexcept { return _future && _future.isFinished(); }

    /// Returns the consumer-facing shared future, for handing the in-flight result to consumers.
    /// Only valid while active(); returns an invalid future otherwise.
    [[nodiscard]] const SharedFuture<R>& future() const noexcept { return _future; }

    /// Eagerly clears the slot (external invalidation / cancellation). Dropping the shared future
    /// requests stop on the underlying task per the framework's demand-counted cancellation.
    void reset() noexcept { _future.reset(); }

    /// Stores the given in-flight operation and arranges for the slot to clear itself once *this*
    /// operation finishes — guarded so that a stale completion hook never clears a newer operation that
    /// may have replaced it in the meantime. Returns the stored shared future for convenient fan-out.
    template<typename Executor>
    const SharedFuture<R>& startAutoReset(SharedFuture<R> future, Executor&& executor) {
        _future = std::move(future);
        installGuardedHook(std::forward<Executor>(executor), [this](Task&) noexcept { _future.reset(); });
        return _future;
    }

    /// Stores the given in-flight operation and runs onFinished once *this* operation finishes — but only
    /// if the slot still holds this same operation (identity guard). The callback typically advances the
    /// slot to the next step (re-launching into this slot) or inspects the outcome. It is not run if the
    /// slot was reset or replaced in the meantime. As with finally(), the callback may optionally accept
    /// the finished Task& (e.g. to inspect its outcome) or take no argument. It must be noexcept (handle
    /// its own exceptions). Returns the stored shared future for convenient fan-out.
    template<typename Executor, typename Function>
    const SharedFuture<R>& startThen(SharedFuture<R> future, Executor&& executor, Function&& onFinished) {
        // The callback runs in a noexcept completion context, so it must handle its own exceptions.
        // This mirrors the contract Task::finally() enforces on its own callbacks.
        static_assert(std::is_nothrow_invocable_r_v<void, Function&> || std::is_nothrow_invocable_r_v<void, Function&, Task&>,
            "OperationSlot::startThen(): the onFinished callback must be noexcept and parameter-free or accept a Task reference.");
        _future = std::move(future);
        installGuardedHook(std::forward<Executor>(executor), std::forward<Function>(onFinished));
        return _future;
    }

private:

    /// Installs the identity-guarded completion hook on the currently stored future.
    template<typename Executor, typename Function>
    void installGuardedHook(Executor&& executor, Function&& onFinished) {
        _future.finally(std::forward<Executor>(executor),
            [this, onFinished = std::forward<Function>(onFinished)](Task& task) mutable noexcept {
                // Run the hook only if the slot still refers to the very task that just finished;
                // otherwise an external path has reset/replaced the operation and the hook is stale.
                if(_future && _future.task().get() == &task) {
                    // Like finally(), accept either a Task&-taking or a nullary callback.
                    if constexpr(std::is_invocable_v<Function&, Task&>)
                        onFinished(task);
                    else
                        onFinished();
                }
            });
    }

    /// The single in-flight operation owned and supervised by this slot.
    SharedFuture<R> _future;
};

}   // End of namespace
