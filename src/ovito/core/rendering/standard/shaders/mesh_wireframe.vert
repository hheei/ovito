// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Non-instanced wireframe: thin 1px lines vertex shader.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

layout(location = 0) in vec3 position;

void main()
{
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(position, 1.0);
}
