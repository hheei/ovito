// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "markers_draw_params.glsl"

// Per-instance marker center position.
layout(location = 0) in vec3 instancePosition;

// Pass instance index to fragment shader for picking ID output.
layout(location = 0) flat out uint instanceIndex_fs;

void main()
{
    // Unit wireframe cube: 12 edges x 2 vertices each = 24 vertices.
    const vec3 boxVerts[24] = vec3[24](
        vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0, -1.0),
        vec3(-1.0, -1.0,  1.0), vec3( 1.0, -1.0,  1.0),
        vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0,  1.0),
        vec3( 1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),
        vec3(-1.0,  1.0, -1.0), vec3( 1.0,  1.0, -1.0),
        vec3(-1.0,  1.0,  1.0), vec3( 1.0,  1.0,  1.0),
        vec3(-1.0,  1.0, -1.0), vec3(-1.0,  1.0,  1.0),
        vec3( 1.0,  1.0, -1.0), vec3( 1.0,  1.0,  1.0),
        vec3(-1.0, -1.0, -1.0), vec3(-1.0,  1.0, -1.0),
        vec3( 1.0, -1.0, -1.0), vec3( 1.0,  1.0, -1.0),
        vec3( 1.0, -1.0,  1.0), vec3( 1.0,  1.0,  1.0),
        vec3(-1.0, -1.0,  1.0), vec3(-1.0,  1.0,  1.0)
    );

    vec4 center_clip = modelViewProjectionMatrix * vec4(instancePosition, 1.0);
    float w = center_clip.w;
    vec3 offset = boxVerts[gl_VertexIndex] * (w * markerSize);

    gl_Position = clipSpaceCorrMatrix * modelViewProjectionMatrix * vec4(instancePosition + offset, 1.0);

    instanceIndex_fs = uint(gl_InstanceIndex);
}
