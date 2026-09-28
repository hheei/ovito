// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Wireframe fragment shader: outputs the uniform wireframe color.
// Shared by all wireframe variants (thin and thick, instanced and non-instanced).

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "mesh_draw_params.glsl"

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = wireframeColor;
}
