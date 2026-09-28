// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: flat-shaded (unlit) arrow billboard.
// Input varyings must match geometry/arrow_flat_triangles.glsl outputs.

layout(location = 0) flat in vec4 color_fs;
layout(location = 1) flat in uint pickingId_fs;

// The shader writes gl_FragDepth = gl_FragCoord.z (the rasterized value), so the
// depth output is unchanged. Declaring this re-enables HiZ / early-Z, which the
// shared cylinder stub's unconditional gl_FragDepth write would otherwise disable.
layout(depth_unchanged) out float gl_FragDepth;

SurfaceSample sampleSurface()
{
    SurfaceSample s;
    s.viewPos   = vec3(0.0);
    s.depth     = gl_FragCoord.z;
    s.normal    = vec3(0.0, 0.0, 1.0);
    s.rayDir    = vec3(0.0, 0.0, -1.0);
    s.color     = color_fs;
    s.pickingId = pickingId_fs;
    return s;
}
