////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

// OIT composite fragment shader.
// Combines the OIT accumulation and reveal buffers into a final composited image
// using the Weighted Blended OIT formula (McGuire & Bavoil).
//
// Final composite: color = accum.rgb / accum.a * (1 - reveal) + background * reveal
// Output: vec4(avgColor, contribution) with standard alpha blend over opaque geometry.

#version 450

layout(binding = 1) uniform sampler2D accumTex;   // RGBA16F: (color*alpha*w, alpha*w)
layout(binding = 2) uniform sampler2D revealTex;  // R8: product of (1 - alpha_i)

layout(location = 0) in vec2 uv_fs;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 accum = texture(accumTex, uv_fs);
    float reveal = texture(revealTex, uv_fs).r;

    // Contribution of transparent geometry = 1 - reveal.
    float contribution = 1.0 - reveal;

    // Skip fully transparent regions to avoid noise from dividing by near-zero.
    if(contribution < 1.0e-4) {
        discard;
    }

    // Compute the average weighted color.
    vec3 avgColor = accum.rgb / max(accum.a, 1.0e-4);

    // Output: the composited transparent color with alpha = contribution.
    // Standard alpha-over blend will composite this over the opaque scene.
    fragColor = vec4(avgColor, contribution);
}
