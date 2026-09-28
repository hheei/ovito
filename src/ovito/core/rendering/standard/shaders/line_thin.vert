// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "lines_draw_params.glsl"

// Per-vertex inputs.
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec4 vertexColor;

// Output to fragment shader.
layout(location = 0) out vec4 color_fs;

void main()
{
    // Apply model-view-projection matrix (identity for pre-projected NDC coordinates).
    // Apply clip-space correction (handles Y-flip for Vulkan/D3D vs OpenGL).
    gl_Position = clipSpaceCorrMatrix * modelViewProjectionMatrix * vec4(vertexPosition, 1.0);

    // Forward vertex color to fragment shader.
    color_fs = vertexColor;
}
