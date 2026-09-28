// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Instanced mesh: visual pass vertex shader (no per-instance colors).
// Per-vertex face color from the mesh is used directly.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-vertex mesh data.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

// Per-instance transformation matrix rows (3 × vec4 = affine 3×4 matrix).
layout(location = 3) in vec4 instTMRow0;
layout(location = 4) in vec4 instTMRow1;
layout(location = 5) in vec4 instTMRow2;

layout(location = 0) out vec4 color_fs;
layout(location = 1) out vec3 normal_vs_fs;

void main()
{
    // Apply per-instance affine transform (3×4 row-major, last row is implicit [0,0,0,1]).
    vec4 posH = vec4(position, 1.0);
    vec3 worldPos = vec3(dot(instTMRow0, posH), dot(instTMRow1, posH), dot(instTMRow2, posH));
    vec4 posVS = modelViewMatrix * vec4(worldPos, 1.0);
    gl_Position = clipProjectionMatrix * posVS;

    // Transform normal: assume affine transform has no non-uniform scale.
    vec4 normalH = vec4(normal, 0.0);
    vec3 worldNormal = vec3(dot(instTMRow0, normalH), dot(instTMRow1, normalH), dot(instTMRow2, normalH));
    normal_vs_fs = mat3(modelViewMatrix) * worldNormal;

    color_fs = color;
}
