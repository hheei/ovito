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

// Geometry snippet: 14-vertex bounding box for the cone-shaped arrow head (NormalShading).
// The bounding box encloses the arrow head cone.
// Output varyings match the inputs of surface/arrow_head_raycast.glsl.

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

const float CONE_RATIO = 1.8;  // Arrow head height / radius ratio.

layout(location = 0) flat out vec4  arrowColor_fs;
layout(location = 1) flat out vec3  coneCenter_fs;    // Arrow tip (head position) in view space
layout(location = 2) flat out vec3  coneAxis_fs;      // Cone axis from tip toward base, in view space
layout(location = 3) flat out float coneRadius_fs;    // Cone base radius in view space
layout(location = 4) flat out uint  pickingId_fs;

void emitVertex(CylinderAttribs a, int vertexIndex)
{
    float radius = 0.5 * a.width;
    float headRadius = radius * 2.5;
    float headLength = CONE_RATIO * headRadius;

    // Total arrow length.
    vec3 arrowVec = a.head - a.base;
    float arrowLen = length(arrowVec);

    // Scale head down if arrow is shorter than the default head length.
    if(arrowLen > 0.0 && headLength > arrowLen) {
        headRadius *= arrowLen / headLength;
        headLength  = arrowLen;
    }

    mat3 ori;
    if(arrowLen > 0.0) {
        ori[2] = arrowVec * (headLength / arrowLen);  // Scaled head axis
        if(ori[2].y != 0.0 || ori[2].x != 0.0)
            ori[0] = normalize(vec3(ori[2].y, -ori[2].x, 0.0)) * headRadius;
        else
            ori[0] = normalize(vec3(-ori[2].z, 0.0, ori[2].x)) * headRadius;
        ori[1] = normalize(cross(ori[2], ori[0])) * headRadius;
    } else {
        ori = mat3(0.0);
    }

    // Cone base starts at head - headAxis.
    vec3 coneBase = a.head - ori[2];
    vec3 corner   = coneBase + ori * UNIT_BOX[vertexIndex];
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(corner, 1.0);

    // Pass view-space cone data to fragment shader.
    coneCenter_fs = (modelViewMatrix * vec4(a.head, 1.0)).xyz;
    // Axis points from tip back toward base.
    coneAxis_fs   = (modelViewMatrix * vec4(-ori[2], 0.0)).xyz;
    coneRadius_fs = headRadius * length(modelViewMatrix[0].xyz);

    arrowColor_fs = a.color1;
    pickingId_fs  = a.pickingId;
}
