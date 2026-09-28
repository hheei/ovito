// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito::detail {

/*
* is_future<T>
*
* Determines whether T is some specialization of the Future, SharedFuture, or ScopedFuture class templates.
*/

template<typename T>
struct is_future : std::false_type {};

template<typename T>
struct is_future<Future<T>> : std::true_type {};

template<typename T>
struct is_future<SharedFuture<T>> : std::true_type {};

template<typename T>
struct is_future<ScopedFuture<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_future_v = is_future<std::decay_t<T>>::value;

/*
* is_shared_future<T>
*
* Determines whether T is some specialization of the SharedFuture class template.
*/

/// Determines whether a type T is some specialization of the SharedFuture class template.
template<typename T>
struct is_shared_future : std::false_type {};

template<typename T>
struct is_shared_future<SharedFuture<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_shared_future_v = is_shared_future<std::decay_t<T>>::value;

/*
* is_scoped_future<T>
*
* Determines whether T is a specialization of the ScopedFuture class template (the owned-scope,
* "join me on cancellation" coroutine return type). Note this matches the static type only: a value
* whose static type is the base Future<T> reports false even if it is a ScopedFuture at runtime.
*/

/// Determines whether a type T is some specialization of the ScopedFuture class template.
template<typename T>
struct is_scoped_future : std::false_type {};

template<typename T>
struct is_scoped_future<ScopedFuture<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_scoped_future_v = is_scoped_future<std::decay_t<T>>::value;

/*
* unwrap_future_t<T>
*
* Yields the value type of a future type (Future<R>, SharedFuture<R>, ScopedFuture<R>) → R, and leaves
* any non-future type unchanged. Used by then() to collapse the continuation's return type to a plain,
* single-consumer Future<R>.
*/
template<typename T>
struct unwrap_future { using type = T; };

template<typename T>
struct unwrap_future<Future<T>> { using type = T; };

template<typename T>
struct unwrap_future<SharedFuture<T>> { using type = T; };

template<typename T>
struct unwrap_future<ScopedFuture<T>> { using type = T; };

template<typename T>
using unwrap_future_t = typename unwrap_future<std::decay_t<T>>::type;

/*
* callable_result<F,FutureType>
*
* Determines the return value type of some callable F, which gets called with the FutureType itself or the results of the future as arguments.
*/

template<typename F, typename FutureType, class = void>
struct callable_result : std::invoke_result<F, typename FutureType::result_type> {};

template<typename F, typename FutureType>
struct callable_result<F, FutureType, std::enable_if_t<!std::is_invocable_v<F, FutureType> && std::is_void_v<typename FutureType::result_type>>> : std::invoke_result<F> {};

template<typename F, typename FutureType>
struct callable_result<F, FutureType, std::enable_if_t<std::is_invocable_v<F, FutureType>>> : std::invoke_result<F, FutureType> {};

template<typename F, typename FutureType>
using callable_result_t = typename callable_result<F, FutureType>::type;

/*
* returns_void<F,FutureType>
*
* Determines some callable F, which gets called with the FutureType itself or the results of the future as arguments, returns void.
*/

/// Determines whether the return type of a callable is 'void'.
template<typename F, typename FutureType>
inline constexpr bool returns_void_v = std::is_void_v<callable_result_t<F, FutureType>>;

/*
* returns_future<F,FutureType>
*
* Determines whether some callable F, which gets called with the FutureType itself or the results of the future as arguments, returns a future.
*/

template<typename F, typename FutureType>
inline constexpr bool returns_future_v = is_future_v<callable_result_t<F, FutureType>>;

/// Determines the Future type that results from a continuation function passed to then().
///
/// then() is the unstructured continuation edge (see ARCHITECTURE.md §4.3): its result is always a
/// single-consumer Future<T>. It never carries the ScopedFuture marker (then() provides no join) and is
/// never a SharedFuture (then() establishes a fresh single-consumer continuation, even when invoked on a
/// SharedFuture). Any future returned by the continuation is unwrapped to its value type.
///
///                 T func(...)   ->   Future<T>
///         Future<T> func(...)   ->   Future<T>  (automatic unwrapping)
///   SharedFuture<T> func(...)   ->   Future<T>  (unwrapped to single-consumer)
///   ScopedFuture<T> func(...)   ->   Future<T>  (scope marker dropped)
///
template<typename F, typename FutureType>
using continuation_future_type = Future<unwrap_future_t<callable_result_t<F,FutureType>>>;

/*
* first_or_void<Ts...>
*
* Yields the first type of a parameter pack, or void if the pack is empty. This encodes the
* "0-or-1 accumulator" convention of for_each_sequential()/reduce_sequential()/executeAsync(): an
* empty result pack means the loop produces no value (a void result), a single type means the loop
* accumulates a value of that type.
*/
template<typename... Ts>
struct first_or_void { using type = void; };

template<typename T, typename... Ts>
struct first_or_void<T, Ts...> { using type = T; };

template<typename... Ts>
using first_or_void_t = typename first_or_void<Ts...>::type;

} // End of namespace
