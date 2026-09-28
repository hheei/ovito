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

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "markers_draw_params.glsl"

// Per-instance marker center position.
layout(location = 0) in vec3 instancePosition;

// Output to fragment shader.
layout(location = 0) out vec4 color_fs;

void main()
{
    // Unit wireframe cube: 12 edges x 2 vertices each = 24 vertices.
    const vec3 boxVerts[24] = vec3[24](
        vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0, -1.0),
        vec3(-1.0, -1.0,  1.0), vec3( 1.0, -1.0,  1.0),
        vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0,  1.0),
        vec3( 1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),
        vec3(-1.0,  1.0, -1.0), vec3( 1.0,  1.0, -1.0),
        vec3(-1.0,  1.0,  1.0), vec3( 1.0,  1.0,  1.0),
        vec3(-1.0,  1.0, -1.0), vec3(-1.0,  1.0,  1.0),
        vec3( 1.0,  1.0, -1.0), vec3( 1.0,  1.0,  1.0),
        vec3(-1.0, -1.0, -1.0), vec3(-1.0,  1.0, -1.0),
        vec3( 1.0, -1.0, -1.0), vec3( 1.0,  1.0, -1.0),
        vec3( 1.0, -1.0,  1.0), vec3( 1.0,  1.0,  1.0),
        vec3(-1.0, -1.0,  1.0), vec3(-1.0,  1.0,  1.0)
    );

    // Project the marker center to clip space to obtain the perspective scale factor W.
    // Scaling the offset by W keeps the marker at a constant apparent screen size.
    vec4 center_clip = modelViewProjectionMatrix * vec4(instancePosition, 1.0);
    float w = center_clip.w;

    // Compute the world-space offset for this box vertex.
    vec3 offset = boxVerts[gl_VertexIndex] * (w * markerSize);

    // Project center + offset, then apply the clip-space correction (Y-flip for Vulkan/D3D).
    gl_Position = clipSpaceCorrMatrix * modelViewProjectionMatrix * vec4(instancePosition + offset, 1.0);

    color_fs = markerColor;
}
