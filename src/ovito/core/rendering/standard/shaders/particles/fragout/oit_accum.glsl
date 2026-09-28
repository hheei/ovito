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

// FragOut snippet: Weighted Blended OIT accumulation pass.
// Applies Phong shading, computes the McGuire & Bavoil weight, writes (color*alpha*w, alpha*w).
// Render with additive blending (src=One, dst=One).

#include "../../shading.glsl"

layout(location = 0) out vec4 accumOut;

void writeOut(SurfaceSample s)
{
    vec4 color = shadedColor(s.color, s.normal, s.rayDir);
    float alpha = color.a;

    // McGuire & Bavoil WBOIT weight: favors near-camera and high-alpha fragments.
    // Use gl_FragCoord.z (rasterizer depth) rather than gl_FragDepth so that surface
    // snippets that don't write a corrected depth (billboard imposters) can omit the
    // gl_FragDepth output entirely, enabling early-Z / HiZ in the opaque passes.
    float z = gl_FragCoord.z;
    float w = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1.0e8 *
                    pow(1.0 - z * 0.9, 3.0), 1.0e-2, 3.0e3);

    accumOut = vec4(color.rgb * alpha * w, alpha * w);
}
