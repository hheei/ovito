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

// Geometry snippet: oriented box / uniform cube triangle strip mesh.
// 14 vertices per instance rendered as TriangleStrip.
// For cube particles, principalAxes = (1,1,1) and orientation = identity quaternion;
// calcShapeOrientation degenerates to radius * identity.
// Output varyings match surface/mesh_surface.glsl inputs.

#include "../shape_orientation.glsl"

layout(location = 0) flat out vec4 color_fs;
layout(location = 1) flat out vec3 normal_view_fs;
layout(location = 5) flat out uint instanceIndex_fs;

const vec3 CUBE_VERTS[14] = vec3[14](
    vec3( 1.0,  1.0,  1.0), vec3( 1.0, -1.0,  1.0), vec3( 1.0,  1.0, -1.0),
    vec3( 1.0, -1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0), vec3( 1.0,  1.0,  1.0), vec3(-1.0,  1.0,  1.0),
    vec3( 1.0,  1.0, -1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0),
    vec3(-1.0,  1.0,  1.0), vec3(-1.0, -1.0,  1.0)
);

// Face normals assigned per provoking-vertex index (first-vertex convention used by Metal/MSL).
// Entry [i] = face normal for triangle T_i; the last two entries are don't-cares.
const vec3 CUBE_NORMALS[14] = vec3[14](
    vec3(1,0,0),  vec3(1,0,0),  vec3(0,0,-1), vec3(0,-1,0),
    vec3(0,-1,0), vec3(0,0,1),  vec3(0,0,1),  vec3(0,1,0),
    vec3(0,1,0),  vec3(0,0,-1), vec3(-1,0,0), vec3(-1,0,0),
    vec3(-1,0,0), vec3(-1,0,0)
);

void emitVertex(ParticleAttribs a, int corner)
{
    mat3 SO = calcShapeOrientation(a.orientation, a.principalAxes, a.radius);
    vec3 worldPos = a.position + SO * CUBE_VERTS[corner];
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(worldPos, 1.0);

    // Normal transform: inverse-transpose of the combined MV*SO matrix.
    // Use adjugate (cross-product of columns) to avoid explicit inverse().
    mat3 MV3 = mat3(modelViewMatrix) * SO;
    mat3 adj = mat3(cross(MV3[1], MV3[2]), cross(MV3[2], MV3[0]), cross(MV3[0], MV3[1]));
    normal_view_fs = normalize(adj * CUBE_NORMALS[corner]);

    color_fs = a.color;
    instanceIndex_fs = a.pickingId;
}
