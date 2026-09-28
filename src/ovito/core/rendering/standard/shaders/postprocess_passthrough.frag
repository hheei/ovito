// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Post-process passthrough fragment shader.
// Samples the intermediate scene color texture and outputs it unchanged.
// This is the base of the post-processing pipeline; outline/highlight effects
// will replace this shader with a more complex one.

#version 450

layout(binding = 1) uniform sampler2D colorTex;  // Intermediate scene color (RGBA8).

layout(location = 0) in vec2 uv_fs;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(colorTex, uv_fs);
}
