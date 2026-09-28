// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include "DeferredObjectExecutor.h"

namespace Ovito {

template<typename Function, typename... Args>
inline void DeferredObjectExecutor::execute(Function&& f, Args&&... args) const& noexcept
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    if(contextObject().expired())
        return;

    Application::instance()->taskManager().submitWork([contextObject = contextObject(), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
        if(OORef<const OvitoObject> target = contextObject.lock())
            std::invoke(std::move(f), std::move(args)...);
    });
}

template<typename Function, typename... Args>
inline void DeferredObjectExecutor::execute(Function&& f, Args&&... args) && noexcept
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    if(contextObject().expired())
        return;

    Application::instance()->taskManager().submitWork([contextObject = std::move(_contextObject), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
        if(OORef<const OvitoObject> target = contextObject.lock())
            std::invoke(std::move(f), std::move(args)...);
    });
}

}   // End of namespace