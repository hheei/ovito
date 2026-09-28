// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito {

/**
 * \brief A simple cache for a single future.
 *
 * This class is used to cache the result of a computation that
 * depends on a single input value (template parameter 'Key').
 * If the input value changes, the cache is invalidated and the
 * caller-provided function is invoked to launch a new computation.
 * If the input value has not changed, the previously computed result is
 * returned in the form of a future and the caller-provided function is not invoked.
 *
 * The caller-provided function must return a Future or SharedFuture object.
 */
template<typename Key>
class FutureCache
{
public:

    template<typename F>
    [[nodiscard]] auto getOrCompute(const Key& key, F&& f) {

        using FutureType = decltype(f());
        using SharedFutureType = SharedFuture<typename FutureType::result_type>;

        if(!_task || _key != key || _task->isCanceled()) {
            _task.reset();
            FutureType future = std::move(f)();
            _key = key;
            _task = future.task();
            return SharedFutureType(std::move(future));
        }
        else {
            return SharedFutureType(_task);
        }
    }

    void reset() {
        _task.reset();
        _key = Key{};
    }

private:

    TaskPtr _task;
    Key _key;
};

}   // End of namespace
