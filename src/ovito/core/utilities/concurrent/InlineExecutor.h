// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/// The simplest implementation of the Executor concept.
/// The inline executor runs a work function immediately and in place.
/// See DeferredObjectExecutor for another implementation of the executor concept.
struct InlineExecutor
{
    template<typename Function, typename... Args>
    static void execute(Function&& f, Args&&... args) noexcept {
        static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
        static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
        static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");
        std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
    }
};

} // End of namespace
