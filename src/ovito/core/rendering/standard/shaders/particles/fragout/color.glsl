// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// FragOut snippet: standard HDR color output with Phong shading.

#include "../../shading.glsl"

layout(location = 0) out vec4 fragColor;

void writeOut(SurfaceSample s)
{
    fragColor = shadedColor(s.color, s.normal, s.rayDir);
}
