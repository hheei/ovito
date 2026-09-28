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

// Geometry snippet: screen-aligned square billboard quad (no UV output).
// Used for SquareCubicShape + FlatShading particles.
// Differs from sphere_imposter_no_depth_quad.glsl in that uv_fs is not emitted —
// this ensures the VS/PS varying interface matches square_billboard.glsl exactly,
// which avoids D3D12 SM6 register-slot linkage errors when PS doesn't read uv_fs.

layout(location = 0) flat out vec4 color_fs;
layout(location = 5) flat out uint instanceIndex_fs;

const vec2 _sqBbQuadCorners[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void emitVertex(ParticleAttribs a, int corner)
{
    vec3 eye_position = (modelViewMatrix * vec4(a.position, 1.0)).xyz;
    float scaledRadius = a.radius * uniformModelScale;

    gl_Position = clipProjectionMatrix *
        vec4(eye_position + vec3(_sqBbQuadCorners[corner] * scaledRadius, 0.0), 1.0);

    color_fs = a.color;
    instanceIndex_fs = a.pickingId;
}
