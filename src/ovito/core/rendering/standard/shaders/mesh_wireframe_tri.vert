// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Non-instanced wireframe: thick lines rendered as screen-aligned triangle-strip quads.
// 4 vertices per instance (one instance = one edge), TriangleStrip topology.
// gl_VertexIndex cycles 0..3 for the four corners of the quad.
//
// Two paths:
//   - Thin path (widthPx <= BRESENHAM_THRESHOLD): pixel-snapped parallelogram aligned
//     to the major screen axis. Reproduces native LINES rasterization rules so each
//     major-axis step covers exactly one minor-axis pixel — pixel-perfect 1px lines.
//   - Thick path: original screen-perpendicular rectangle. Preserves CAD-style stroke
//     semantics for visibly thick wireframes.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-instance: edge start and end in object space (two consecutive entries in wireframe VBO).
layout(location = 0) in vec3 position_from;
layout(location = 1) in vec3 position_to;

const float BRESENHAM_THRESHOLD = 1.5;

void main()
{
    int corner = gl_VertexIndex;

    vec4 projFrom = clipProjectionMatrix * modelViewMatrix * vec4(position_from, 1.0);
    vec4 projTo   = clipProjectionMatrix * modelViewMatrix * vec4(position_to,   1.0);

    float widthPx = lineThickness * viewportHeight;
    bool crossesCamera = (projTo.w * projFrom.w < 0.0);
    bool useBresenham = (widthPx <= BRESENHAM_THRESHOLD) && !crossesCamera;

    vec4 expandedPos;

    if(useBresenham) {
        // Pixel-space coordinates of the endpoints.
        vec2 viewport = vec2(viewportWidth, viewportHeight);
        vec2 fromPx = (projFrom.xy / projFrom.w * 0.5 + 0.5) * viewport;
        vec2 toPx   = (projTo.xy   / projTo.w   * 0.5 + 0.5) * viewport;

        // On Y-up-NDC APIs (Metal, D3D) the formula above produces a bottom-up pixel Y.
        // Flip to top-down so the floor() snap below hits the correct screen pixel row.
        if(isYUpInNDC != 0) {
            fromPx.y = viewport.y - fromPx.y;
            toPx.y   = viewport.y - toPx.y;
        }

        // Snap endpoints to pixel centers — eliminates sub-pixel jitter that would
        // otherwise cause partial coverage of two adjacent rows.
        fromPx = floor(fromPx) + 0.5;
        toPx   = floor(toPx)   + 0.5;

        // Parallelogram offset: along the minor screen axis only. This guarantees
        // each major-axis column (or row) covers exactly one minor-axis pixel, matching
        // the diamond-exit rule used by hardware LINES rasterization.
        vec2 d = toPx - fromPx;
        vec2 offset = (abs(d.x) >= abs(d.y))
            ? vec2(0.0, widthPx * 0.5)
            : vec2(widthPx * 0.5, 0.0);

        vec2 endpointPx = (corner < 2) ? fromPx : toPx;
        float sgn = ((corner & 1) == 0) ? -1.0 : 1.0;
        vec2 cornerPx = endpointPx + sgn * offset;

        // Flip back to the API-native pixel Y before converting to NDC.
        if(isYUpInNDC != 0)
            cornerPx.y = viewport.y - cornerPx.y;

        // Back to NDC, then to clip space.
        vec2 cornerNDC = cornerPx / viewport * 2.0 - 1.0;
        float wOut = (corner < 2) ? projFrom.w : projTo.w;
        float zOut = (corner < 2) ? projFrom.z : projTo.z;
        expandedPos = vec4(cornerNDC * wOut, zOut, wOut);
    }
    else {
        // Original screen-perpendicular rectangle.
        vec2 delta = normalize(projTo.xy / projTo.w - projFrom.xy / projFrom.w) * lineThickness;

        if(crossesCamera)
            delta = -delta;

        delta.y *= viewportHeight / viewportWidth;

        if(corner == 0)
            expandedPos = projFrom - vec4( delta.y * projFrom.w, -delta.x * projFrom.w, 0.0, 0.0);
        else if(corner == 1)
            expandedPos = projFrom + vec4( delta.y * projFrom.w, -delta.x * projFrom.w, 0.0, 0.0);
        else if(corner == 2)
            expandedPos = projTo   - vec4( delta.y * projTo.w,   -delta.x * projTo.w,   0.0, 0.0);
        else
            expandedPos = projTo   + vec4( delta.y * projTo.w,   -delta.x * projTo.w,   0.0, 0.0);
    }

    gl_Position = expandedPos;
}
