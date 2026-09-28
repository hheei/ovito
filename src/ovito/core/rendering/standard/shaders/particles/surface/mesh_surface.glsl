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

// Surface snippet: mesh surface for cube/box particles.
// Reads the precomputed view-space face normal from the geometry stage.
// Input varyings must match the output declarations in geometry/box_oriented_mesh.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) flat in vec3 normal_view_fs;
layout(location = 5) flat in uint instanceIndex_fs;

SurfaceSample sampleSurface()
{
    vec3 rayDir;
    if(isPerspective != 0) {
        float ndc_x = (gl_FragCoord.x / viewportWidth) * 2.0 - 1.0;
        float ndc_y_raw = gl_FragCoord.y / viewportHeight;
        float ndc_y = (isYUpInFramebuffer != 0) ? (ndc_y_raw * 2.0 - 1.0) : (1.0 - ndc_y_raw * 2.0);
        rayDir = normalize(vec3(ndc_x / projectionMatrix[0][0], ndc_y / projectionMatrix[1][1], -1.0));
    } else {
        rayDir = vec3(0.0, 0.0, -1.0);
    }

    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.normal    = normal_view_fs;
    s.rayDir    = rayDir;
    s.color     = color_fs;
    s.pickingId = instanceIndex_fs;
    return s;
}
