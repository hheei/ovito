// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

class OVITO_CORE_EXPORT ThreadPoolExecutor
{
public:

    /// Constructor.
    explicit ThreadPoolExecutor(bool highPriority = false) noexcept : _highPriority(highPriority) {}

    /// Executes some work.
    template<typename Function>
    void execute(Function&& f) const noexcept;

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) const noexcept {
        static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
        static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
        static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");
        execute(std::bind_front(std::forward<Function>(f), std::forward<Args>(args)...));
    }

private:

    bool _highPriority;
};

}   // End of namespace
