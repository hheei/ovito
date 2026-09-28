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

// Surface snippet: flat-shaded screen-aligned square billboard.
// No circle discard; depth taken from the rasterizer.
// Setting normal=(0,0,1) makes shadedColor() return full-brightness (= flat) color.
// Input varyings must match geometry/sphere_imposter_no_depth_quad.glsl (uv_fs unused here).
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 5) flat in uint instanceIndex_fs;

SurfaceSample sampleSurface()
{
    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.normal    = vec3(0.0, 0.0, 1.0);
    s.rayDir    = vec3(0.0, 0.0, -1.0);
    s.color     = color_fs;
    s.pickingId = instanceIndex_fs;
    return s;
}
