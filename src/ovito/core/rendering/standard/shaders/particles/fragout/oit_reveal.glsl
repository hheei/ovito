// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// FragOut snippet: Weighted Blended OIT reveal pass.
// Writes transparency (1 - alpha) to all channels of revealOut.
// Render with multiplicative blending (src=Zero, dst=SrcColor).
// The blend equation accumulates: reveal = product(1 - alpha_i) across overlapping fragments.

layout(location = 0) out vec4 revealOut;

void writeOut(SurfaceSample s)
{
    float transparency = 1.0 - s.color.a;
    revealOut = vec4(transparency, transparency, transparency, transparency);
}
