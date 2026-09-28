// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Instanced wireframe: thin 1px lines vertex shader.
// Each mesh instance renders all wireframe edges transformed by its instance TM.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-vertex wireframe endpoint.
layout(location = 0) in vec3 position;

// Per-instance transformation matrix rows.
layout(location = 1) in vec4 instTMRow0;
layout(location = 2) in vec4 instTMRow1;
layout(location = 3) in vec4 instTMRow2;

void main()
{
    vec4 posH = vec4(position, 1.0);
    vec3 worldPos = vec3(dot(instTMRow0, posH), dot(instTMRow1, posH), dot(instTMRow2, posH));
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(worldPos, 1.0);
}
