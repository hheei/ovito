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

// Surface snippet: flat-shaded (unlit) arrow billboard.
// Input varyings must match geometry/arrow_flat_triangles.glsl outputs.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) flat in uint pickingId_fs;

// The shader writes gl_FragDepth = gl_FragCoord.z (the rasterized value), so the
// depth output is unchanged. Declaring this re-enables HiZ / early-Z, which the
// shared cylinder stub's unconditional gl_FragDepth write would otherwise disable.
layout(depth_unchanged) out float gl_FragDepth;

SurfaceSample sampleSurface()
{
    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.depth     = gl_FragCoord.z;
    s.normal    = vec3(0.0, 0.0, 1.0);
    s.rayDir    = vec3(0.0, 0.0, -1.0);
    s.color     = color_fs;
    s.pickingId = pickingId_fs;
    return s;
}
