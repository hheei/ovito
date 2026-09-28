// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Instanced mesh: picking pass vertex shader.
// One picking ID per mesh instance (= gl_InstanceIndex).

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-vertex mesh data.
layout(location = 0) in vec3 position;

// Per-instance transformation matrix rows.
layout(location = 3) in vec4 instTMRow0;
layout(location = 4) in vec4 instTMRow1;
layout(location = 5) in vec4 instTMRow2;

layout(location = 0) flat out uint primitiveId_fs;

void main()
{
    vec4 posH = vec4(position, 1.0);
    vec3 worldPos = vec3(dot(instTMRow0, posH), dot(instTMRow1, posH), dot(instTMRow2, posH));
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(worldPos, 1.0);
    primitiveId_fs = uint(gl_InstanceIndex);
}
