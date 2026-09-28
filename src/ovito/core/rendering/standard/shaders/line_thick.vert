// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "lines_draw_params.glsl"

// Per-instance inputs: start and end positions of the line segment.
layout(location = 0) in vec3 position_from;
layout(location = 1) in vec3 position_to;

// Per-instance inputs: colors at the start and end of the line segment.
layout(location = 2) in vec4 color_from;
layout(location = 3) in vec4 color_to;

// Output to fragment shader.
layout(location = 0) out vec4 color_fs;

void main()
{
    // gl_VertexIndex cycles 0..3 for the four quad corners of the triangle strip.
    int corner = gl_VertexIndex;

    // Project both line endpoints to clip space (without clip-space correction).
    // For pre-projected NDC coordinates, modelViewProjectionMatrix is identity.
    vec4 proj_from = modelViewProjectionMatrix * vec4(position_from, 1.0);
    vec4 proj_to   = modelViewProjectionMatrix * vec4(position_to,   1.0);

    // Compute the line direction in NDC and scale by the line half-thickness.
    vec2 delta = normalize(proj_to.xy / proj_to.w - proj_from.xy / proj_from.w) * lineThickness;

    // Correct sign if one endpoint is behind the near plane (w < 0).
    if(proj_to.w * proj_from.w < 0.0)
        delta = -delta;

    // Correct for the viewport aspect ratio so the line width is uniform in screen pixels.
    // (NDC x spans viewportWidth pixels, NDC y spans viewportHeight pixels.)
    delta.y *= viewportHeight / viewportWidth;

    // Expand the line endpoints into a screen-aligned quad.
    // The perpendicular direction (rotated 90°) is (-delta.y, delta.x).
    // Multiply by w to convert from NDC to clip space.
    vec4 expandedPos;
    if(corner == 0)
        expandedPos = proj_from - vec4( delta.y * proj_from.w, -delta.x * proj_from.w, 0.0, 0.0);
    else if(corner == 1)
        expandedPos = proj_from + vec4( delta.y * proj_from.w, -delta.x * proj_from.w, 0.0, 0.0);
    else if(corner == 2)
        expandedPos = proj_to   - vec4( delta.y * proj_to.w,   -delta.x * proj_to.w,   0.0, 0.0);
    else
        expandedPos = proj_to   + vec4( delta.y * proj_to.w,   -delta.x * proj_to.w,   0.0, 0.0);

    // Apply clip-space correction (handles Y-flip for Vulkan/D3D vs OpenGL).
    gl_Position = clipSpaceCorrMatrix * expandedPos;

    // Interpolate color: corners 0/1 get color_from, corners 2/3 get color_to.
    color_fs = (corner < 2) ? color_from : color_to;
}
