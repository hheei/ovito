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
