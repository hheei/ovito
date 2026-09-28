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
#include "lines_draw_params.glsl"

// Per-instance inputs: start and end positions only (no colors in picking pass).
layout(location = 0) in vec3 position_from;
layout(location = 1) in vec3 position_to;

// Pass instance index to fragment shader to compute picking primitive ID.
layout(location = 0) flat out uint instanceIndex_fs;

void main()
{
    int corner = gl_VertexIndex;

    vec4 proj_from = modelViewProjectionMatrix * vec4(position_from, 1.0);
    vec4 proj_to   = modelViewProjectionMatrix * vec4(position_to,   1.0);

    vec2 delta = normalize(proj_to.xy / proj_to.w - proj_from.xy / proj_from.w) * lineThickness;

    if(proj_to.w * proj_from.w < 0.0)
        delta = -delta;

    delta.y *= viewportHeight / viewportWidth;

    vec4 expandedPos;
    if(corner == 0)
        expandedPos = proj_from - vec4( delta.y * proj_from.w, -delta.x * proj_from.w, 0.0, 0.0);
    else if(corner == 1)
        expandedPos = proj_from + vec4( delta.y * proj_from.w, -delta.x * proj_from.w, 0.0, 0.0);
    else if(corner == 2)
        expandedPos = proj_to   - vec4( delta.y * proj_to.w,   -delta.x * proj_to.w,   0.0, 0.0);
    else
        expandedPos = proj_to   + vec4( delta.y * proj_to.w,   -delta.x * proj_to.w,   0.0, 0.0);

    gl_Position = clipSpaceCorrMatrix * expandedPos;

    // Each instance is one line segment; pass the instance index for picking ID computation.
    instanceIndex_fs = uint(gl_InstanceIndex);
}
