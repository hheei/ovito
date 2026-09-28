// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform ImageParams {
    vec4 ndcRect;               // (x_min, y_min, x_max, y_max) in NDC
    int isYUpInNDC;
    int _pad0; int _pad1; int _pad2;
};

void main()
{
    // Triangle strip: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right.
    float u = float(gl_VertexIndex & 1);
    float v = float(gl_VertexIndex >> 1);

    float px = mix(ndcRect.x, ndcRect.z, u);
    float py = mix(ndcRect.y, ndcRect.w, v);
    gl_Position = vec4(px, py, 0.0, 1.0);

    // Texture coordinate: u=0..1 left-to-right, v=0..1 top-to-bottom in QImage.
    // For y-up NDC backends (OpenGL, Metal): v=0 maps to screen bottom, so we must flip tv
    // so that the top of the image (texture v=0) appears at the top of the screen (NDC y_max).
    // For y-down NDC backends (Vulkan): v=0 already maps to screen top, no flip needed.
    float tv = v;
    if(isYUpInNDC != 0)
        tv = 1.0 - tv;
    v_texcoord = vec2(u, tv);
}
