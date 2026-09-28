// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Non-instanced mesh: visual pass vertex shader.
// Per-vertex attributes match MeshPrimitive::RenderVertex layout.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

layout(location = 0) out vec4  color_fs;
layout(location = 1) out vec3  normal_vs_fs;

void main()
{
    vec4 posVS = modelViewMatrix * vec4(position, 1.0);
    gl_Position = clipProjectionMatrix * posVS;
    color_fs = color;
    normal_vs_fs = mat3(modelViewMatrix) * normal;
}
