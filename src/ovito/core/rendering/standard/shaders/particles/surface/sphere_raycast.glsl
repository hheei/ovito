// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: ray-sphere intersection for raycast sphere fragments.
// Input varyings must match the output declarations in geometry/sphere_raycast_quad.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) flat in vec3 particle_view_pos_fs;
layout(location = 2) flat in float particle_radius_squared_fs;
layout(location = 3) in vec3 ray_origin_fs;
layout(location = 4) in vec3 ray_dir_fs;
layout(location = 5) flat in uint instanceIndex_fs;

#include "../../shading.glsl"

// The geometry stage shifts the quad along the camera ray onto the sphere's
// front-cap plane (see sphere_raycast_quad.glsl), so the rasterized depth is
// always ≤ the analytic surface depth. depth_greater re-enables HiZ for
// Vulkan/Metal; the HLSL variant is patched at build time to plain SV_Depth
// (SPIRV-Cross omits noperspective_centroid on SV_Position when SV_DepthGreaterEqual
// is output, failing DXIL validation on Direct3D).
#ifndef OVITO_DISABLE_DEPTH_GREATER
layout(depth_greater) out float gl_FragDepth;
#else
out float gl_FragDepth;
#endif

SurfaceSample sampleSurface()
{
    vec3 ray_dir_norm = normalize(ray_dir_fs);
    vec3 sphere_dir = particle_view_pos_fs - ray_origin_fs;
    float b = dot(ray_dir_norm, sphere_dir);
    vec3 delta = ray_dir_norm * b - sphere_dir;
    float disc = particle_radius_squared_fs - dot(delta, delta);

    // Merge both miss conditions into a single discard: spirv-cross HLSL emitter
    // fails with "argument pulled into unrelated predicate" when a function contains
    // two separate OpKill instructions in different selection constructs.
    // max(disc,0) keeps sqrt defined when the ray misses the sphere.
    float tnear = b - sqrt(max(disc, 0.0));
    if(disc < 0.0 || (isPerspective != 0 && tnear < 0.0)) discard;

    vec3 hit = ray_origin_fs + tnear * ray_dir_norm;
    float ze = hit.z;
    float zn = (projectionMatrix[2][2] * ze + projectionMatrix[3][2]) /
               (projectionMatrix[2][3] * ze + projectionMatrix[3][3]);

    SurfaceSample s;
    s.viewPos    = hit;
    s.normal     = normalize(hit - particle_view_pos_fs);
    s.rayDir     = ray_dir_norm;
    s.color      = color_fs;
    s.pickingId  = instanceIndex_fs;
    gl_FragDepth = zn * 0.5 + 0.5;
    return s;
}
