// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Geometry snippet: sphere imposter billboard quad with depth correction.
// The fragment shader reconstructs z from the imposter's UV coordinates.
// Output varyings must match the input declarations in surface/sphere_imposter_with_depth.glsl.

layout(location = 0) flat out vec4 color_fs;
layout(location = 1) out vec2 uv_fs;
layout(location = 2) flat out vec2 radius_and_eyez_fs;
layout(location = 5) flat out uint instanceIndex_fs;

const vec2 _impWDQuadCorners[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void emitVertex(ParticleAttribs a, int corner)
{
    vec3 eye_position = (modelViewMatrix * vec4(a.position, 1.0)).xyz;
    float scaledRadius = a.radius * uniformModelScale;

    vec3 quadPos = eye_position + vec3(_impWDQuadCorners[corner] * scaledRadius, 0.0);

    // Shift the quad along the camera ray onto the sphere's front-cap plane.
    // In perspective, scaling along the ray from the origin preserves NDC xy
    // (both points project to the same screen position) and overrides only NDC z.
    // In ortho, the rays are parallel to the view axis so it's a pure z shift.
    // After the shift, the analytic surface depth (computed in the fragment
    // shader from radius_and_eyez_fs.y, which is still the sphere center's eye-z)
    // is always ≥ the rasterized depth, which makes layout(depth_greater) safe
    // and re-enables HiZ / early-Z.
    if(isPerspective != 0) {
        float sphere_dist = length(eye_position);
        float frontZ = eye_position.z * (1.0 - scaledRadius / sphere_dist);
        quadPos *= frontZ / quadPos.z;
    } else {
        quadPos.z += scaledRadius;
    }

    gl_Position = clipProjectionMatrix * vec4(quadPos, 1.0);

    color_fs = a.color;
    uv_fs = _impWDQuadCorners[corner];
    radius_and_eyez_fs = vec2(scaledRadius, eye_position.z);
    instanceIndex_fs = a.pickingId;
}
