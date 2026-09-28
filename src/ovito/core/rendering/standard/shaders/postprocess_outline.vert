// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Post-process outline/highlight vertex shader.
// Generates a fullscreen triangle from three vertices (no vertex buffer needed).

#version 450

layout(std140, binding = 0) uniform PostProcessParams {
    int   isYUpInNDC;
    int   hasHighlight;
    int   silhouetteWidth;
    int   outlineEnabled;
    vec4  outlineColor;
    int   outlineWidth;
    float minDepthDiff;
    float maxDepthDiff;
    int   isPerspective;
    float nearPlane;
    float farPlane;
    int   _padDepthRange; // unused (was depthZeroToOne)
    int   isYUpInFramebuffer;
    int   outlineMinWidth;
    int   _pad0, _pad1, _pad2;
    vec4  highlightColor;  // color for HighlightLayer silhouette outline (always red)
};

layout(location = 0) out vec2 uv_fs;

void main()
{
    // Generate a fullscreen triangle from vertex IDs 0, 1, 2.
    // The triangle covers [-1,-1]..[3,3] in NDC; the rasteriser clips it to [-1,1]^2.
    // No Y-flip of gl_Position is needed — the triangle works the same in all backends.
    vec2 pos = vec2(
        float((gl_VertexIndex & 1) != 0) * 4.0 - 1.0,
        float((gl_VertexIndex & 2) != 0) * 4.0 - 1.0
    );

    gl_Position = vec4(pos, 0.0, 1.0);

    // Map NDC [-1,1] → UV [0,1].
    uv_fs = pos * 0.5 + 0.5;

    // Metal has Y-up NDC (like OpenGL) but Y-down texture storage (like Vulkan/D3D).
    // In that case isYUpInNDC==1 and isYUpInFramebuffer==0, so the UV needs a vertical flip.
    // On OpenGL both are 1; on Vulkan both are 0 — no flip needed in either case.
    if(isYUpInNDC != isYUpInFramebuffer)
        uv_fs.y = 1.0 - uv_fs.y;
}
