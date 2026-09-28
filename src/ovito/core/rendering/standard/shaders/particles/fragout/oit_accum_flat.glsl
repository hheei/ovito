// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// FragOut snippet: Weighted Blended OIT accumulation pass for flat shaded imposters.
// Computes the McGuire & Bavoil weight, writes (color*alpha*w, alpha*w).
// Render with additive blending (src=One, dst=One).

#include "../../shading.glsl"

layout(location = 0) out vec4 accumOut;

void writeOut(SurfaceSample s)
{
    vec4 color = s.color;
    float alpha = color.a;

    // McGuire & Bavoil WBOIT weight: favors near-camera and high-alpha fragments.
    // Use gl_FragCoord.z (rasterizer depth) rather than gl_FragDepth — see oit_accum.glsl.
    float z = gl_FragCoord.z;
    float w = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1.0e8 *
                    pow(1.0 - z * 0.9, 3.0), 1.0e-2, 3.0e3);

    accumOut = vec4(color.rgb * alpha * w, alpha * w);
}
