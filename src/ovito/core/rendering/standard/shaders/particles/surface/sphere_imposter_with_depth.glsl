// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: sphere imposter with per-fragment depth correction.
// Input varyings must match the output declarations in geometry/sphere_imposter_with_depth_quad.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) in vec2 uv_fs;
layout(location = 2) flat in vec2 radius_and_eyez_fs;
layout(location = 5) flat in uint instanceIndex_fs;

#include "../../shading.glsl"

// The geometry stage shifts the quad along the camera ray onto the sphere's
// front-cap plane (see sphere_imposter_with_depth_quad.glsl), so the rasterized
// depth is always ≤ the analytic surface depth. depth_greater re-enables HiZ for
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
    float rsq = dot(uv_fs, uv_fs);
    if(rsq >= 1.0) discard;

    vec3 surface_normal = vec3(uv_fs, sqrt(1.0 - rsq));
    float ze = radius_and_eyez_fs.y + surface_normal.z * radius_and_eyez_fs.x;
    float zn = (projectionMatrix[2][2] * ze + projectionMatrix[3][2]) /
               (projectionMatrix[2][3] * ze + projectionMatrix[3][3]);

    SurfaceSample s;
    s.viewPos    = vec3(0.0); // not used downstream
    s.normal     = surface_normal;
    s.rayDir     = vec3(0.0, 0.0, -1.0);
    s.color      = color_fs;
    s.pickingId  = instanceIndex_fs;
    gl_FragDepth = zn * 0.5 + 0.5;
    return s;
}
