// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Geometry snippet: sphere imposter billboard quad without depth correction.
// The rasterizer's interpolated z is used as depth; no per-fragment depth computation.
// Output varyings must match the input declarations in surface/sphere_imposter_no_depth.glsl.

layout(location = 0) flat out vec4 color_fs;
layout(location = 1) out vec2 uv_fs;
layout(location = 5) flat out uint instanceIndex_fs;

const vec2 _impNDQuadCorners[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void emitVertex(ParticleAttribs a, int corner)
{
    vec3 eye_position = (modelViewMatrix * vec4(a.position, 1.0)).xyz;
    float scaledRadius = a.radius * uniformModelScale;

    gl_Position = clipProjectionMatrix *
        vec4(eye_position + vec3(_impNDQuadCorners[corner] * scaledRadius, 0.0), 1.0);

    color_fs = a.color;
    uv_fs = _impNDQuadCorners[corner];
    instanceIndex_fs = a.pickingId;
}
