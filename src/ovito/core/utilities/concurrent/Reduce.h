// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "ForEach.h"

namespace Ovito {

template<typename ResultType, typename InputRange, class Executor, typename Function>
[[nodiscard]] auto reduce_sequential(ResultType&& initialResultValue, InputRange&& inputRange, Executor&& executor, Function&& f)
{
    return for_each_sequential(
        std::forward<InputRange>(inputRange),
        std::forward<Executor>(executor),
        // Iteration start function:
        std::forward<Function>(f),
        // Iteration completed function (a no-op):
        [](typename InputRange::const_reference iterValue) noexcept {},
        std::forward<ResultType>(initialResultValue));
}

}   // End of namespace
