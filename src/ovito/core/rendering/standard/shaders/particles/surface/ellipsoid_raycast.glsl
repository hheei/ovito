// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: ray-ellipsoid intersection.
// Transforms the view ray into the unit-sphere space defined by the shape orientation,
// performs ray-sphere intersection there, then maps the result back to view space.
// Input varyings must match the output declarations in geometry/ellipsoid_raycast_box.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0)  flat in vec4 color_fs;
layout(location = 1)  flat in mat3 view_to_sphere_fs;   // locations 1,2,3
layout(location = 4)  flat in mat3 sphere_to_view_fs;   // locations 4,5,6
layout(location = 7)  flat in vec3 particle_view_pos_fs;
layout(location = 8)  noperspective in vec3 ray_origin_fs;
layout(location = 9)  in vec3 ray_dir_fs;
layout(location = 10) flat in uint instanceIndex_fs;

#include "../../shading.glsl"

// The rasterized geometry is a back-face-culled bounding box that fully encloses
// the ellipsoid, so the analytic surface depth is always ≥ the front-face depth.
// depth_greater re-enables HiZ for Vulkan/Metal; the HLSL variant is patched at
// build time to plain SV_Depth (SPIRV-Cross omits noperspective_centroid on SV_Position
// when SV_DepthGreaterEqual is output, failing DXIL validation on Direct3D).
#ifndef OVITO_DISABLE_DEPTH_GREATER
layout(depth_greater) out float gl_FragDepth;
#else
out float gl_FragDepth;
#endif

SurfaceSample sampleSurface()
{
    vec3 ray_dir_norm = normalize(ray_dir_fs);

    // Transform ray into unit-sphere space.
    vec3 sphere_dir = view_to_sphere_fs * (particle_view_pos_fs - ray_origin_fs);
    vec3 ray_dir2   = normalize(view_to_sphere_fs * ray_dir_norm);

    // Ray-sphere intersection in unit-sphere space.
    float b    = dot(ray_dir2, sphere_dir);
    vec3 delta = ray_dir2 * b - sphere_dir;
    float disc = 1.0 - dot(delta, delta);
    float tnear = b - sqrt(max(disc, 0.0));
    if(disc < 0.0 || (isPerspective != 0 && tnear < 0.0)) discard;

    // Intersection point in sphere space, then transform back to view space.
    vec3 sphere_hit = tnear * ray_dir2 - sphere_dir;
    vec3 view_hit   = sphere_to_view_fs * sphere_hit + particle_view_pos_fs;

    float ze = view_hit.z;
    float zn = (projectionMatrix[2][2] * ze + projectionMatrix[3][2]) /
               (projectionMatrix[2][3] * ze + projectionMatrix[3][3]);

    // Normal: sphere-space gradient of the unit sphere (= sphere_hit itself) transformed
    // by the transpose of view_to_sphere_fs (= inverse-transpose of sphere_to_view_fs).
    // In GLSL, (rowVec * M) is equivalent to (transpose(M) * colVec).
    vec3 surface_normal = normalize(sphere_hit * view_to_sphere_fs);

    SurfaceSample s;
    s.viewPos   = view_hit;
    s.normal    = surface_normal;
    s.rayDir    = ray_dir_norm;
    s.color     = color_fs;
    s.pickingId = instanceIndex_fs;
    gl_FragDepth = zn * 0.5 + 0.5;
    return s;
}
