// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Post-process passthrough vertex shader.
// Generates a fullscreen triangle from three vertices (no vertex buffer needed).

#version 450

layout(std140, binding = 0) uniform PostProcessParams {
    int isYUpInNDC;
    int _pad0;
    int _pad1;
    int _pad2;
};

layout(location = 0) out vec2 uv_fs;

void main()
{
    // Generate a fullscreen triangle from vertex IDs 0, 1, 2.
    vec2 pos = vec2(
        float((gl_VertexIndex & 1) != 0) * 4.0 - 1.0,
        float((gl_VertexIndex & 2) != 0) * 4.0 - 1.0
    );

    // UV coordinates in [0,1] range.
    uv_fs = pos * 0.5 + 0.5;

    // Account for Y-up/Y-down differences in NDC vs framebuffer.
    if(isYUpInNDC == 0)
        pos.y = -pos.y;

    gl_Position = vec4(pos, 0.0, 1.0);
}
