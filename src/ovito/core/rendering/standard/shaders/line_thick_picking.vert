// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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
