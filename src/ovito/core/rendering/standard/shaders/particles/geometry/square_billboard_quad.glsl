// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Geometry snippet: screen-aligned square billboard quad (no UV output).
// Used for SquareCubicShape + FlatShading particles.
// Differs from sphere_imposter_no_depth_quad.glsl in that uv_fs is not emitted —
// this ensures the VS/PS varying interface matches square_billboard.glsl exactly,
// which avoids D3D12 SM6 register-slot linkage errors when PS doesn't read uv_fs.

layout(location = 0) flat out vec4 color_fs;
layout(location = 5) flat out uint instanceIndex_fs;

const vec2 _sqBbQuadCorners[4] = vec2[4](
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
        vec4(eye_position + vec3(_sqBbQuadCorners[corner] * scaledRadius, 0.0), 1.0);

    color_fs = a.color;
    instanceIndex_fs = a.pickingId;
}
