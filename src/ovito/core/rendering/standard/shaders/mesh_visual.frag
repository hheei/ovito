// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Mesh visual fragment shader: Phong shading with view-space normal.
// Shared by all visual (non-OIT, non-picking) mesh variants.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "shading.glsl"

layout(location = 0) in vec4 color_fs;
layout(location = 1) in vec3 normal_vs_fs;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 ray_dir;
    if(isPerspective != 0) {
        float ndcX = gl_FragCoord.x / viewportWidth  * 2.0 - 1.0;
        float ndcY = gl_FragCoord.y / viewportHeight * 2.0 - 1.0;
        if(isYUpInFramebuffer == 0) ndcY = -ndcY;
        ray_dir = normalize(vec3(ndcX / projectionMatrix[0][0], ndcY / projectionMatrix[1][1], -1.0));
    } else {
        ray_dir = vec3(0.0, 0.0, -1.0);
    }
    fragColor = shadedColor(color_fs, normalize(normal_vs_fs), ray_dir);
}
