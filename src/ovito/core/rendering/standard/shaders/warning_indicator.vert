// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform IndicatorParams {
    float x;                // NDC x of left edge of icon quad
    float y_top;            // NDC y of top edge (pre-computed for the current API's Y convention)
    float w;                // NDC width (positive)
    float y_bot;            // NDC y of bottom edge (pre-computed for the current API's Y convention)
    int isYUpInFramebuffer; // 1 = OpenGL (v=0 at bottom), 0 = Vulkan/D3D/Metal (v=0 at top)
    int _pad0;
    int _pad1;
    int _pad2;
};

void main()
{
    float px = x + w * float(gl_VertexIndex & 1);
    // y_top and y_bot are pre-computed by the CPU for the correct NDC convention.
    // Vertex order: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right (triangle strip).
    float py = (gl_VertexIndex >> 1) == 0 ? y_top : y_bot;
    gl_Position = vec4(px, py, 0.0, 1.0);

    // Texture coordinate: u=0..1 left-to-right, v=0..1 top-to-bottom.
    // For y-up framebuffer (OpenGL): OpenGL textures have v=0 at the bottom, so flip v.
    // For y-down framebuffer (Vulkan/D3D/Metal): v=0 is naturally at the top, no flip.
    float u = float(gl_VertexIndex & 1);
    float v = float(gl_VertexIndex >> 1);
    if(isYUpInFramebuffer != 0)
        v = 1.0 - v;
    v_texcoord = vec2(u, v);
}
