// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "ScopedFuture.h"
#include "CoroutinePromise.h"

namespace Ovito {

namespace detail {

/// Invokes the for_each_sequential() start function for one item, passing the accumulator as a second
/// argument if (and only if) the function accepts it. Returns the future produced by the function.
template<typename StartFn, typename Item, typename Acc>
[[nodiscard]] decltype(auto) forEachInvokeStart(StartFn& startFunc, Item&& item, Acc& accumulator) {
    if constexpr(std::is_invocable_v<StartFn&, Item, Acc&>)
        return startFunc(std::forward<Item>(item), accumulator);
    else
        return startFunc(std::forward<Item>(item));
}

/// Overload for the void-accumulator case: the loop has no result value, so the start function is
/// always called with just the item.
template<typename StartFn, typename Item>
[[nodiscard]] decltype(auto) forEachInvokeStart(StartFn& startFunc, Item&& item, std::nullptr_t) {
    return startFunc(std::forward<Item>(item));
}

/// Invokes the for_each_sequential() complete function for one item with the largest argument list it
/// accepts: (item, result, accumulator&), (item, result), or just (item). The 'result' pack is empty
/// when the iteration future carried no value.
template<typename CompleteFn, typename Item, typename Acc, typename... Result>
void forEachInvokeComplete(CompleteFn& completeFunc, Item&& item, Acc& accumulator, Result&&... result) {
    if constexpr(std::is_invocable_v<CompleteFn&, Item, Result..., Acc&>)
        completeFunc(std::forward<Item>(item), std::forward<Result>(result)..., accumulator);
    else if constexpr(std::is_invocable_v<CompleteFn&, Item, Result...>)
        completeFunc(std::forward<Item>(item), std::forward<Result>(result)...);
    else
        completeFunc(std::forward<Item>(item));
}

/// Overload for the void-accumulator case: complete is called as (item, result) or just (item).
template<typename CompleteFn, typename Item, typename... Result>
void forEachInvokeComplete(CompleteFn& completeFunc, Item&& item, std::nullptr_t, Result&&... result) {
    if constexpr(std::is_invocable_v<CompleteFn&, Item, Result...>)
        completeFunc(std::forward<Item>(item), std::forward<Result>(result)...);
    else
        completeFunc(std::forward<Item>(item));
}

/// Computes the adaptive return type of for_each_sequential(): the loop is an owned scope (ScopedFuture)
/// if and only if the per-iteration sub-tasks produced by the start function are themselves ScopedFutures;
/// otherwise it is a self-contained Future. As a transparent forwarder, the combinator adopts a scoped
/// return type to forward its scoped children's join obligation up to its own caller (ARCHITECTURE.md
/// §4.3), while joining those children on cancellation via the child-gated rule and detaching from
/// plain/shared ones.
template<typename InputRange, typename StartIterFunc, typename... ResultType>
struct for_each_return {
    using result_type = first_or_void_t<std::decay_t<ResultType>...>;
    /// The start function is invoked with just the item for a void-result loop, otherwise with
    /// (item, accumulator&); mirror forEachInvokeStart's third argument here to deduce the sub-task type.
    using accumulator_arg = std::conditional_t<std::is_void_v<result_type>, std::nullptr_t, std::add_lvalue_reference_t<result_type>>;
    using iteration_future = std::decay_t<decltype(
        forEachInvokeStart(std::declval<StartIterFunc&>(),
                           *std::begin(std::declval<InputRange&>()),
                           std::declval<accumulator_arg>()))>;
    using type = std::conditional_t<is_scoped_future_v<iteration_future>, ScopedFuture<result_type>, Future<result_type>>;
};

template<typename InputRange, typename StartIterFunc, typename... ResultType>
using for_each_return_t = typename for_each_return<InputRange, StartIterFunc, ResultType...>::type;

} // namespace detail

/**
 * \brief Asynchronously iterates over a range, running one sub-task per item in sequence.
 *
 * For each item the \a startFunc is called to launch an asynchronous sub-task; once that sub-task
 * completes, \a completeFunc processes its result before the loop proceeds to the next item. The loop
 * therefore runs strictly one item at a time. It is OVITO's structured async "for-each": when the
 * per-iteration sub-tasks are ScopedFutures the loop is a Tier-1 owned scope (see ARCHITECTURE.md §4.1)
 * and joins them on cancellation; for plain Future or shared sub-tasks it detaches instead.
 *
 * Optionally the loop carries an accumulator value: pass a single \a initialResult and the result is
 * yielded by the returned future (this is what reduce_sequential() builds on). With no \a initialResult
 * the loop produces no value (a void result).
 *
 * Call conventions (detected automatically):
 *  - \a startFunc is invoked as <tt>startFunc(item)</tt> or, if it accepts it, <tt>startFunc(item, accumulator&)</tt>.
 *    It must return a Future/SharedFuture/ScopedFuture (whose result may be void).
 *  - \a completeFunc is invoked with the largest argument list it accepts among
 *    <tt>completeFunc(item, result, accumulator&)</tt>, <tt>completeFunc(item, result)</tt>, or <tt>completeFunc(item)</tt>.
 *
 * \a executor selects where each iteration runs and resumes (typically a DeferredObjectExecutor, so
 * the loop body runs deferred on the main thread). An exception thrown by either function, or an error
 * from an awaited sub-task, completes the returned future on the error channel.
 *
 * The return type is adaptive (see ARCHITECTURE.md §4.3): the loop returns a ScopedFuture when the
 * per-iteration sub-tasks are ScopedFutures (it is then an owned scope that joins them on cancellation),
 * and a plain Future otherwise (it detaches from self-contained or shared sub-tasks).
 */
template<typename InputRange, class Executor, typename StartIterFunc, typename CompleteIterFunc, typename... ResultType>
[[nodiscard]] detail::for_each_return_t<InputRange, StartIterFunc, ResultType...>
for_each_sequential(
    InputRange inputRange,
    Executor executor,
    StartIterFunc startFunc,
    CompleteIterFunc completeFunc,
    ResultType... initialResult)
{
    using result_type = detail::first_or_void_t<std::decay_t<ResultType>...>;

    if constexpr(std::is_void_v<result_type>) {
        if(std::begin(inputRange) != std::end(inputRange)) {
            // Hop onto the (deferred) executor so even the first iteration runs there, preserving the
            // deferred-execution contract of the loop.
            co_await ExecutorAwaiter(Executor(executor));
            for(auto&& item : inputRange) {
                auto iterationFuture = detail::forEachInvokeStart(startFunc, item, nullptr);
                if constexpr(std::is_void_v<typename decltype(iterationFuture)::result_type>) {
                    co_await FutureAwaiter(Executor(executor), std::move(iterationFuture));
                    detail::forEachInvokeComplete(completeFunc, item, nullptr);
                }
                else {
                    auto iterationResult = co_await FutureAwaiter(Executor(executor), std::move(iterationFuture));
                    detail::forEachInvokeComplete(completeFunc, item, nullptr, std::move(iterationResult));
                }
            }
        }
        co_return;
    }
    else {
        result_type accumulator{std::move(initialResult)...};
        if(std::begin(inputRange) != std::end(inputRange)) {
            co_await ExecutorAwaiter(Executor(executor));
            for(auto&& item : inputRange) {
                auto iterationFuture = detail::forEachInvokeStart(startFunc, item, accumulator);
                if constexpr(std::is_void_v<typename decltype(iterationFuture)::result_type>) {
                    co_await FutureAwaiter(Executor(executor), std::move(iterationFuture));
                    detail::forEachInvokeComplete(completeFunc, item, accumulator);
                }
                else {
                    auto iterationResult = co_await FutureAwaiter(Executor(executor), std::move(iterationFuture));
                    detail::forEachInvokeComplete(completeFunc, item, accumulator, std::move(iterationResult));
                }
            }
        }
        co_return accumulator;
    }
}

}   // End of namespace
