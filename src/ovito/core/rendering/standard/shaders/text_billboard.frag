// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 2) uniform sampler2D atlasTex;

void main()
{
    vec4 color = texture(atlasTex, v_texcoord);
    // Discard fully transparent texels to save blending work in the large empty
    // regions of the label quads.
    if(color.a < 0.003)
        discard;
    fragColor = color;
}
