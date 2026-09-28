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
#include "ScopedFuture.h"
#include "CoroutinePromise.h"

namespace Ovito {

namespace detail {

/**
 * \brief Coroutine awaiter that waits for a future's task to finish *without consuming the future*.
 *
 * A normal FutureAwaiter takes ownership of the awaited future and extracts its result on resume.
 * whenAll() instead needs to wait for each future in a range while leaving the range's futures intact,
 * so the caller can read their results afterwards. This awaiter takes the awaited task dependency out
 * of the future for the duration of the wait, then rebuilds the (now finished) future in place from the
 * completed task — the result is preserved.
 */
template<typename FutureType>
class RewindingFutureAwaiter
{
    Q_DISABLE_COPY_MOVE(RewindingFutureAwaiter)

    static_assert(is_future_v<FutureType>, "RewindingFutureAwaiter can only be used with Future or SharedFuture types");

public:

    explicit RewindingFutureAwaiter(FutureType& slot) noexcept : _slot(slot) {}

    bool await_ready() const noexcept { return _slot.isFinished(); }

    template<typename R, bool SC>
    void await_suspend(std::coroutine_handle<CoroutinePromise<R, SC>> handle) {
        auto coroTask = handle.promise().coroTask();
        // Same rule as FutureAwaiter (see ScopedFuture.h and ARCHITECTURE.md §4.3): join-vs-detach is
        // decided by the awaited element's declared type alone — a ScopedFuture element is joined on
        // cancellation (the awaiting frame is kept alive until it finishes), while plain Future and shared
        // SharedFuture elements are detached. whenAll() forwards a scoped element's join obligation up to
        // its own caller through its adaptive return type (when_all_return_t), not through this per-element edge.
        constexpr bool JoinOnCancel = is_scoped_future_v<FutureType>;
        coroTask->template whenTaskFinishes<JoinOnCancel>(_slot.takeTaskDependency(), InlineExecutor{}, std::move(handle.promise()),
            [this](PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
                // Rebuild the future in place from the finished task, preserving its result.
                _slot = FutureType{std::move(finishedTask)};
                auto coroTask = static_cast<CoroutineTask<R, SC>*>(promise.task().get());
                if(!coroTask->isCanceled())
                    coroTask->resumeCoroutine(std::move(promise));
            });
    }

    void await_resume() const noexcept {}

private:

    FutureType& _slot;
};

/// Computes the adaptive return type of whenAll(): a ScopedFuture when the range's elements are
/// ScopedFutures (the combinator is then an owned scope joining them on cancellation), a plain Future
/// otherwise. A combinator is a transparent forwarder — its scoped elements typically borrow from the
/// combinator's *caller* — so it adopts a scoped return type to forward that join obligation upward
/// (ARCHITECTURE.md §4.3), while joining the elements themselves via the child-gated rule above.
template<typename InputRange>
using when_all_return_t = std::conditional_t<
    is_scoped_future_v<std::decay_t<decltype(*std::begin(std::declval<std::decay_t<InputRange>&>()))>>,
    ScopedFuture<std::decay_t<InputRange>>, Future<std::decay_t<InputRange>>>;

} // namespace detail

/// Asynchronously waits for all futures in the given range to complete (or to get canceled), then yields
/// the same range back with every future finished and readable. The return type is adaptive: when the
/// range holds ScopedFutures it is a Tier-1 owned scope that joins them on cancellation; for plain Future
/// or shared elements it detaches instead.
template<typename InputRange>
[[nodiscard]] detail::when_all_return_t<InputRange> whenAll(InputRange range)
{
    for(auto& slot : range) {
        OVITO_ASSERT(slot);
        co_await detail::RewindingFutureAwaiter<std::decay_t<decltype(slot)>>{slot};
    }
    co_return std::move(range);
}

}   // End of namespace
