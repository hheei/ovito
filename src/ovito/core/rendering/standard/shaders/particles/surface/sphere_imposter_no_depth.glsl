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

// Surface snippet: sphere imposter without depth correction.
// Depth comes from the rasterizer's interpolated z (gl_FragCoord.z).
// Input varyings must match the output declarations in geometry/sphere_imposter_no_depth_quad.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) in vec2 uv_fs;
layout(location = 5) flat in uint instanceIndex_fs;

#include "../../shading.glsl"

SurfaceSample sampleSurface()
{
    float rsq = dot(uv_fs, uv_fs);
    if(rsq >= 1.0) discard;

    vec3 surface_normal = vec3(uv_fs, sqrt(1.0 - rsq));

    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.normal    = surface_normal;
    s.rayDir    = vec3(0.0, 0.0, -1.0);
    s.color     = color_fs;
    s.pickingId = instanceIndex_fs;
    return s;
}
