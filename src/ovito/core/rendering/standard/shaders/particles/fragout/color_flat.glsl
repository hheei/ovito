// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// FragOut snippet: flat (unlit) color output — no Phong shading applied.
// Used for the no-depth imposter sphere in the standard color pass (fast/low-quality mode).
// Declares fragColor and implements writeOut(SurfaceSample).

layout(location = 0) out vec4 fragColor;

void writeOut(SurfaceSample s)
{
    fragColor = s.color;
}
