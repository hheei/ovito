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
#include "SharedFuture.h"
#include "WeakSharedFuture.h"

namespace Ovito {

/**
 * \brief A shared, memoized, demand-stopped asynchronous value — the "shared" tier of the async
 *        framework (see ARCHITECTURE.md, §4 Tier 2).
 *
 * A SharedAsyncValue is the registry slot for an eagerly-started result that multiple consumers, living
 * in *different* scopes, attach to — the pipeline-evaluation pattern. It is the structured-concurrency
 * escape hatch (the analogue of std::execution's `split`/`ensure_started`): a DAG node with many
 * independent "parents" that cannot live inside a single Tier-1 TaskScope tree.
 *
 * It names and formalizes a pattern OVITO already implements, combining three existing properties:
 *  - **Weak registry, not owner.** It holds a WeakSharedFuture, so it provides *discovery* (attach to
 *    an in-flight computation) without *ownership*. A strong reference here would leak unstructured
 *    background work, so the producer must be kept alive by its consumers, never by this slot.
 *  - **Demand-counted lifetime/stop.** The producer is alive ⇔ at least one consumer still demands it
 *    (the detail::TaskDependency / `_dependentsCount` mechanism, acting here as a *merged* stop source:
 *    "stop only once all consumers have stopped"). When the last consumer drops its SharedFuture the
 *    producer is canceled.
 *  - **Fan-out value/error.** Completion replays to every current consumer (SharedFuture).
 *
 * **Detach, don't join.** A consumer that holds a SharedFuture obtained from attach() simply *drops* it
 * when done (O(1), non-blocking) — it must never *join* (block waiting for) a shared producer that other
 * consumers may still demand. This is the rule that distinguishes the shared tier from a Tier-1 owned
 * child (which is joined on scope exit). Dropping the SharedFuture is exactly what ~detail::TaskDependency
 * does: it decrements demand and, only when demand reaches zero, requests stop.
 *
 * The single subtlety this type encapsulates is the **race-safe attach**: WeakSharedFuture::lock() can
 * succeed on a producer whose task has *already* been canceled (a consumer dropped the last demanding
 * reference, requesting stop, but the weak entry has not yet expired). attach() returns such a producer
 * as *unavailable*, so the caller relaunches a fresh one rather than handing back a value that is
 * mid-stop.
 */
template<typename R>
class SharedAsyncValue
{
public:

    /// Constructs an empty slot, not associated with any producer.
    SharedAsyncValue() noexcept = default;

    /// Constructs a slot memoizing the given producer (stored as a weak, non-owning reference).
    SharedAsyncValue(const SharedFuture<R>& producer) noexcept : _producer(producer) {}

    /// Memoizes the given producer (stored as a weak, non-owning reference), replacing any previous one.
    SharedAsyncValue& operator=(const SharedFuture<R>& producer) noexcept {
        _producer = producer;
        return *this;
    }

    /// Race-safe attach: returns a live SharedFuture sharing the memoized producer, or an invalid
    /// SharedFuture if none is currently available — i.e. the slot is empty, the producer has expired
    /// (no consumer left), or it has already been canceled (mid-stop). In the latter cases the caller
    /// should start a fresh producer.
    [[nodiscard]] SharedFuture<R> attach() const noexcept {
        SharedFuture<R> producer = _producer.lock();
        if(producer && !producer.isCanceled())
            return producer;
        return SharedFuture<R>();
    }

    /// Returns whether no consumer is currently keeping the producer alive (demand has reached zero).
    /// Useful for evicting dead slots from a registry.
    [[nodiscard]] bool expired() const noexcept { return _producer.expired(); }

    /// Forgets the memoized producer.
    void reset() noexcept { _producer.reset(); }

    /// Cancels the memoized producer, if it is still running, and forgets it.
    /// Use this instead of reset() when the *inputs* of the ongoing computation have changed. Forgetting
    /// the producer alone does not stop it: demand is held by the consumers, so the task keeps running and
    /// installs its now outdated result when it finishes. Requesting stop here makes the in-flight task
    /// wind down at its next cancellation check, and any consumer awaiting it is canceled along with it.
    void cancel_and_reset() noexcept {
        SharedFuture<R> producer = _producer.lock();
        _producer.reset();
        if(producer)
            producer.task()->cancel();
    }

    /// Attaches to the memoized producer if one is available, otherwise starts a fresh producer via the
    /// given launcher and memoizes it. The launcher must return a SharedFuture<R>. This is the
    /// split/ensure_started building block for callers whose attach-or-relaunch logic is self-contained;
    /// callers with more elaborate bookkeeping can use attach() plus assignment directly.
    template<typename Launcher>
    [[nodiscard]] SharedFuture<R> get_or_start(Launcher&& launcher) {
        if(SharedFuture<R> existing = attach())
            return existing;
        SharedFuture<R> fresh = std::forward<Launcher>(launcher)();
        _producer = fresh;
        return fresh;
    }

private:

    /// The weak (non-owning) reference to the memoized producer. Demand is held by the consumers.
    WeakSharedFuture<R> _producer;
};

}   // End of namespace
