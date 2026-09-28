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

// Geometry snippet: 14-vertex bounding box for ray-cylinder intersection (NormalShading cylinders).
// The box is oriented along the cylinder axis and scaled by the cylinder radius.
// gl_VertexIndex in [0..13] addresses the triangle-strip box corner.
// Output varyings match the inputs of surface/cylinder_raycast.glsl.

// Bounding box for a unit cylinder (height = axis length, radius = 1) as a 14-vertex triangle strip.
// These are the same corner offsets used by the legacy OpenGL cylinder shader.
const vec3 UNIT_BOX[14] = vec3[14](
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3( 1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0),
    vec3(-1.0, -1.0,  0.0),
    vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  0.0),
    vec3(-1.0,  1.0,  0.0),
    vec3(-1.0, -1.0,  0.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0, -1.0,  1.0)
);

layout(location = 0) flat out vec4 color1_fs;
layout(location = 1) flat out vec4 color2_fs;
layout(location = 2) flat out vec3 cylinderViewBase_fs;
layout(location = 3) flat out vec3 cylinderViewAxis_fs;
layout(location = 4) flat out float cylinderRadiusSq_fs;
layout(location = 5) flat out float cylinderLength_fs;
layout(location = 6) flat out uint pickingId_fs;

void emitVertex(CylinderAttribs a, int vertexIndex)
{
    float radius = 0.5 * a.width;

    // Build an axis-aligned tripod in object space.
    vec3 axis = a.head - a.base;
    mat3 ori;
    ori[2] = axis;  // Cylinder axis (full length, unnormalized).
    if(axis != vec3(0.0)) {
        if(axis.y != 0.0 || axis.x != 0.0)
            ori[0] = normalize(vec3(axis.y, -axis.x, 0.0)) * radius;
        else
            ori[0] = normalize(vec3(-axis.z, 0.0, axis.x)) * radius;
        ori[1] = normalize(cross(axis, ori[0])) * radius;
    } else {
        ori = mat3(0.0);
    }

    // Transform bounding box corner to clip space.
    vec3 corner = a.base + ori * UNIT_BOX[vertexIndex];
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(corner, 1.0);

    // Pass view-space cylinder geometry to fragment shader.
    cylinderViewBase_fs = (modelViewMatrix * vec4(a.base, 1.0)).xyz;
    vec3 viewAxis        = (modelViewMatrix * vec4(axis, 0.0)).xyz;
    cylinderViewAxis_fs = viewAxis;
    cylinderLength_fs   = length(viewAxis);

    // Scale radius by modelView uniform scale (modelViewMatrix[0] column length).
    float vsRadius = radius * length(modelViewMatrix[0].xyz);
    cylinderRadiusSq_fs = vsRadius * vsRadius;

    color1_fs  = a.color1;
    color2_fs  = a.color2;
    pickingId_fs = a.pickingId;
}
