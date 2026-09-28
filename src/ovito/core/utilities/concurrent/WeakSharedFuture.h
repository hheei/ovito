// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "SharedFuture.h"

namespace Ovito {

/**
 * A weak reference to a SharedFuture
 */
template<typename R>
class WeakSharedFuture : private std::weak_ptr<Task>
{
public:

#ifndef Q_CC_MSVC
    constexpr WeakSharedFuture() noexcept = default;
#else
    constexpr WeakSharedFuture() noexcept : std::weak_ptr<Task>() {}
#endif

    WeakSharedFuture(const SharedFuture<R>& future) noexcept : std::weak_ptr<Task>(future.task()) {}

    WeakSharedFuture& operator=(const SharedFuture<R>& f) noexcept {
        std::weak_ptr<Task>::operator=(f.task());
        return *this;
    }

    void reset() noexcept {
        std::weak_ptr<Task>::reset();
    }

    SharedFuture<R> lock() const noexcept {
        return SharedFuture<R>(std::weak_ptr<Task>::lock());
    }

    bool expired() const noexcept {
        return std::weak_ptr<Task>::expired();
    }
};

}   // End of namespace
