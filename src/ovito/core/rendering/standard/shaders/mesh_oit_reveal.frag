// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Mesh OIT reveal fragment shader.
// Outputs alpha transparency to the reveal buffer.
// Render with multiplicative blending (src=Zero, dst=SrcColor).

#version 450

layout(location = 0) in vec4 color_fs;
layout(location = 1) in vec3 normal_vs_fs;

layout(location = 0) out vec4 revealOut;

void main()
{
    float transparency = 1.0 - color_fs.a;
    revealOut = vec4(transparency, transparency, transparency, transparency);
}
