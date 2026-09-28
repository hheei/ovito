// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/**
 * POD struct carrying outline post-processing parameters from the GUI thread to the
 * RenderThread. The parameters are owned by the RenderSettings object, because the effect is
 * implemented by all rendering backends alike and should not be lost when the user switches
 * to a different one. RenderSettings hands them to the FrameGraph, from where each renderer's
 * createConfiguration() method copies them into its Configuration.
 */
struct OutlineSettings
{
    bool      enabled         = false;
    FloatType minDepthDiff    = FloatType(0.5);
    FloatType maxDepthDiff    = std::numeric_limits<FloatType>::infinity();
    FloatType minOutlineWidth = FloatType(1);
    FloatType maxOutlineWidth = FloatType(4);
    bool      useCustomColor  = false;
    Color     customColor     = Color(0, 0, 0);
};

}  // namespace Ovito
