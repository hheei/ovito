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
#include "Future.h"

namespace Ovito {

/**
 * \brief The return type of a coroutine that forms an owned structured-concurrency scope.
 *
 * Returning a ScopedFuture (rather than a plain Future) is how a coroutine declares "I must be joined
 * on cancellation" — typically because it borrows state from its parent across a suspension point. A
 * plain Future is the self-contained, droppable default. The marker is read off the awaited handle: in
 * FutureAwaiter (see CoroutinePromise.h) the join-vs-detach flag is `is_scoped_future_v<awaited>` — the
 * awaiting coroutine joins exactly its ScopedFuture children (keeping its own frame alive until they
 * finish) and detaches from plain Futures and shared SharedFutures. The decision is made per await edge
 * from the *child's* declared type alone.
 *
 * A coroutine's own return type is an orthogonal declaration about a *different* edge — whether it
 * borrows from *its* parent — so awaiting a ScopedFuture child does not, by itself, require the awaiting
 * coroutine to be a ScopedFuture. A coroutine that borrows from its parent across a suspension declares
 * so by returning a ScopedFuture; its parent then joins it by the same child-gated rule. The chain of
 * ScopedFuture returns therefore extends upward only as far as borrowed state actually reaches,
 * terminating at the first coroutine that owns everything it touches. See ARCHITECTURE.md §4.3.
 *
 * Apart from selecting the structured promise type, a ScopedFuture behaves like a Future<R> for its
 * consumer and carries no extra state — except that then()/postprocess() are deliberately deleted on it
 * (they are unstructured continuation edges; lower to Future first or consume with co_await).
 */
template<typename R>
class ScopedFuture : public Future<R>
{
    Q_DISABLE_COPY(ScopedFuture)

public:

    /// The promise type for C++ coroutines returning a ScopedFuture (structured-concurrency mode).
    using promise_type = CoroutinePromise<R, true>;

    /// Inherit constructors from base class.
    using Future<R>::Future;

    /// Move constructor.
    ScopedFuture(ScopedFuture&& other) noexcept : Future<R>(static_cast<Future<R>&&>(other)) {}

    /// Conversion constructor from Future<R> to ScopedFuture<R>.
    ScopedFuture(Future<R> future) noexcept : Future<R>(std::move(future)) {}

    /// Move assignment operator.
    ScopedFuture& operator=(ScopedFuture&& other) noexcept {
        Future<R>::operator=(static_cast<Future<R>&&>(other));
        return *this;
    }

    /// Conversion assignment operator from Future<R> to ScopedFuture<R>.
    ScopedFuture& operator=(Future<R> future) noexcept {
        Future<R>::operator=(std::move(future));
        return *this;
    }

    /// then()/postprocess() are deliberately removed on ScopedFuture. They form the *unstructured*
    /// continuation edge (a detached continuation, see ARCHITECTURE.md §4.3), which cannot honor a
    /// scope's join-on-cancellation guarantee. Consume a scope structurally with co_await, or lower it
    /// to a plain Future<R> explicitly (entering the unstructured world) before attaching a continuation.
    template<typename Executor, typename Function> auto then(Executor&&, Function&&) = delete;
    template<typename Function> auto then(Function&&) = delete;
    template<typename Executor, typename Function> void postprocess(Executor&&, Function&&) = delete;
};

}   // End of namespace
