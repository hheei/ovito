// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// OIT composite vertex shader.
// Generates a fullscreen triangle from three vertices (no vertex buffer needed).

#version 450
#extension GL_GOOGLE_include_directive : enable

#include "scene_params.glsl"

layout(location = 0) out vec2 uv_fs;

void main()
{
    // Generate a fullscreen triangle from vertex IDs 0, 1, 2.
    // Triangle covers the full NDC range [-1, 1] with a 2x2 quad approach.
    vec2 pos = vec2(
        float((gl_VertexIndex & 1) != 0) * 4.0 - 1.0,
        float((gl_VertexIndex & 2) != 0) * 4.0 - 1.0
    );

    // UV coordinates in [0,1] range.
    uv_fs = pos * 0.5 + 0.5;

    // Metal has Y-up NDC but Y-down framebuffer storage; flip UV.y to compensate.
    // On D3D/Vulkan both conventions match; on OpenGL both are Y-up — no flip in either case.
    if(isYUpInNDC != isYUpInFramebuffer)
        uv_fs.y = 1.0 - uv_fs.y;

    // No Y-flip of gl_Position needed: the fullscreen triangle covers the viewport
    // identically in all backends, regardless of NDC Y direction.
    gl_Position = vec4(pos, 0.0, 1.0);
}
