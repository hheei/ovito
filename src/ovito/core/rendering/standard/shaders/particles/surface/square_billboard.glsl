// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: flat-shaded screen-aligned square billboard.
// No circle discard; depth taken from the rasterizer.
// Setting normal=(0,0,1) makes shadedColor() return full-brightness (= flat) color.
// Input varyings must match geometry/sphere_imposter_no_depth_quad.glsl (uv_fs unused here).
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0) flat in vec4 color_fs;
layout(location = 5) flat in uint instanceIndex_fs;

SurfaceSample sampleSurface()
{
    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.normal    = vec3(0.0, 0.0, 1.0);
    s.rayDir    = vec3(0.0, 0.0, -1.0);
    s.color     = color_fs;
    s.pickingId = instanceIndex_fs;
    return s;
}
