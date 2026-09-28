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

// Geometry snippet: 14-vertex bounding box for the cylindrical arrow tail (NormalShading).
// The tail runs from the base to (head - headLength).
// Output varyings match the inputs of surface/arrow_tail_raycast.glsl.

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

const float CONE_RATIO = 1.8;

layout(location = 0) flat out vec4  arrowColor_fs;
layout(location = 1) flat out vec3  cylinderViewBase_fs;
layout(location = 2) flat out vec3  cylinderViewAxis_fs;
layout(location = 3) flat out float cylinderRadiusSq_fs;
layout(location = 4) flat out float cylinderLength_fs;
layout(location = 5) flat out uint  pickingId_fs;

void emitVertex(CylinderAttribs a, int vertexIndex)
{
    float radius     = 0.5 * a.width;
    float headRadius = radius * 2.5;
    float headLength = CONE_RATIO * headRadius;

    vec3 arrowVec = a.head - a.base;
    float arrowLen = length(arrowVec);

    if(arrowLen > 0.0 && headLength > arrowLen) {
        headRadius *= arrowLen / headLength;
        headLength  = arrowLen;
    }

    // The tail cylinder runs from base to (head - headLength * direction).
    vec3 tailHead = (arrowLen > 0.0) ? (a.head - arrowVec * (headLength / arrowLen)) : a.base;
    vec3 tailAxis = tailHead - a.base;

    mat3 ori;
    ori[2] = tailAxis;
    if(tailAxis != vec3(0.0)) {
        if(tailAxis.y != 0.0 || tailAxis.x != 0.0)
            ori[0] = normalize(vec3(tailAxis.y, -tailAxis.x, 0.0)) * radius;
        else
            ori[0] = normalize(vec3(-tailAxis.z, 0.0, tailAxis.x)) * radius;
        ori[1] = normalize(cross(tailAxis, ori[0])) * radius;
    } else {
        ori = mat3(0.0);
    }

    vec3 corner = a.base + ori * UNIT_BOX[vertexIndex];
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(corner, 1.0);

    cylinderViewBase_fs = (modelViewMatrix * vec4(a.base, 1.0)).xyz;
    vec3 viewAxis       = (modelViewMatrix * vec4(tailAxis, 0.0)).xyz;
    cylinderViewAxis_fs = viewAxis;
    cylinderLength_fs   = length(viewAxis);
    float vsRadius      = radius * length(modelViewMatrix[0].xyz);
    cylinderRadiusSq_fs = vsRadius * vsRadius;

    arrowColor_fs = a.color1;
    pickingId_fs  = a.pickingId;
}
