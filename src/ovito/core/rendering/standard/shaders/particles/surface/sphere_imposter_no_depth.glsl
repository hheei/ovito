// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: sphere imposter without depth correction.
// Depth comes from the rasterizer's interpolated z (gl_FragCoord.z).
// Input varyings must match the output declarations in geometry/sphere_imposter_no_depth_quad.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) in vec2 uv_fs;
layout(location = 5) flat in uint instanceIndex_fs;

#include "../../shading.glsl"

SurfaceSample sampleSurface()
{
    float rsq = dot(uv_fs, uv_fs);
    if(rsq >= 1.0) discard;

    vec3 surface_normal = vec3(uv_fs, sqrt(1.0 - rsq));

    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.normal    = surface_normal;
    s.rayDir    = vec3(0.0, 0.0, -1.0);
    s.color     = color_fs;
    s.pickingId = instanceIndex_fs;
    return s;
}
