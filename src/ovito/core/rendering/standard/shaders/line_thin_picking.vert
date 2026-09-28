// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "lines_draw_params.glsl"

// Per-vertex position input.
layout(location = 0) in vec3 vertexPosition;

// Pass vertex index to fragment shader to compute line segment index.
layout(location = 0) flat out uint vertexIndex_fs;

void main()
{
    // Apply model-view-projection matrix (identity for pre-projected NDC coordinates).
    gl_Position = clipSpaceCorrMatrix * modelViewProjectionMatrix * vec4(vertexPosition, 1.0);

    // Each line segment uses 2 consecutive vertices; the fragment shader divides by 2 to get the segment index.
    vertexIndex_fs = uint(gl_VertexIndex);
}
