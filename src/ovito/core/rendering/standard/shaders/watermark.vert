// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform WatermarkParams {
    float renderWidth;       // Render target width in pixels
    float renderHeight;      // Render target height in pixels
    float watermarkWidth;    // Watermark texture width in pixels
    float watermarkHeight;   // Watermark texture height in pixels
    int isYUpInNDC;          // 1 = Metal/D3D/OpenGL (NDC y=+1 at top), 0 = Vulkan (NDC y=-1 at top)
    int isYUpInFramebuffer;  // 1 = OpenGL (framebuffer v=0 at bottom), 0 = Vulkan/Metal/D3D
    int _pad0;
    int _pad1;
};

void main()
{
    // u,v in [0,1]: u=0 left, u=1 right; v=0 top, v=1 bottom (screen space).
    // Vertex order as triangle strip: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right.
    float u = float(gl_VertexIndex & 1);
    float v = float((gl_VertexIndex >> 1) & 1);

    // NDC position accounts for each backend's y-up vs y-down convention.
    float px = u * 2.0 - 1.0;
    float py = (isYUpInNDC != 0) ? (1.0 - v * 2.0) : (v * 2.0 - 1.0);
    gl_Position = vec4(px, py, 0.0, 1.0);

    // Tiled UV: pixel_position / watermark_size.
    // The Repeat sampler handles tiling automatically when UV exceeds [0,1].
    float scaleU = renderWidth  / watermarkWidth;
    float scaleV = renderHeight / watermarkHeight;
    v_texcoord = vec2(u * scaleU, v * scaleV);

    // OpenGL stores textures bottom-up; flip V so tile origin is at the top-left of the screen.
    if(isYUpInFramebuffer != 0)
        v_texcoord.y = scaleV - v_texcoord.y;
}
