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

// Geometry snippet: 15-vertex TriangleList for flat-shaded (billboard) arrow glyphs.
// Represents a 7-vertex triangle fan (5 triangles = 15 vertices) without using TriangleFan topology
// to maintain compatibility with all QRhi backends (Metal, Vulkan do not support TriangleFan).
//
// Fan layout (2D in object-space cylinder plane):
//   vertex 0: tip (1.0, 0.0)
//   vertex 1: (1-L, +H)   upper arrow head shoulder
//   vertex 2: (1-L, +1.0) upper body shoulder
//   vertex 3: (0.0, +1.0) upper body back
//   vertex 4: (0.0, -1.0) lower body back
//   vertex 5: (1-L, -1.0) lower body shoulder
//   vertex 6: (1-L, -H)   lower arrow head shoulder
// Triangles: (0,1,2), (0,2,3), (0,3,4), (0,4,5), (0,5,6) — always with vertex 0 as hub.

layout(location = 0) flat out vec4 color_fs;
layout(location = 1) flat out uint pickingId_fs;

void emitVertex(CylinderAttribs a, int vertexIndex)
{
    float radius = 0.5 * a.width;

    // Build view-aligned coordinate frame in object space.
    vec3 viewDir = (viewDirEyePos.w != 0.0) ? (viewDirEyePos.xyz - a.base) : viewDirEyePos.xyz;
    vec3 axisVec = a.head - a.base;
    vec3 sideDir = normalize(cross(viewDir, axisVec)) * radius;

    float arrowHeadRadius = 2.5;
    float arrowLen = length(axisVec);
    float arrowHeadLength = (arrowLen > 0.0) ? (radius * arrowHeadRadius * 1.8 / arrowLen) : 0.0;

    // Decode (triIndex, vtxInTri) from flat vertexIndex.
    int triIndex  = vertexIndex / 3;
    int vtxInTri  = vertexIndex % 3;
    // Fan vertex index: hub = 0 for vtxInTri==0, otherwise fan[triIndex + vtxInTri].
    int fanIndex = (vtxInTri == 0) ? 0 : (triIndex + vtxInTri);

    // Compute 2D position in the (axisVec, sideDir) plane.
    vec2 vpos;
    if(arrowHeadLength < 1.0) {
        float L = arrowHeadLength;
        float H = arrowHeadRadius;
        if     (fanIndex == 0) vpos = vec2( 1.0,  0.0);
        else if(fanIndex == 1) vpos = vec2( 1.0 - L,  H);
        else if(fanIndex == 2) vpos = vec2( 1.0 - L,  1.0);
        else if(fanIndex == 3) vpos = vec2( 0.0,  1.0);
        else if(fanIndex == 4) vpos = vec2( 0.0, -1.0);
        else if(fanIndex == 5) vpos = vec2( 1.0 - L, -1.0);
        else                   vpos = vec2( 1.0 - L, -H);
    } else {
        // Arrow head fills the whole arrow.
        float H = arrowHeadRadius / arrowHeadLength;
        if     (fanIndex == 0) vpos = vec2( 1.0,  0.0);
        else if(fanIndex == 1) vpos = vec2( 0.0,  H);
        else if(fanIndex == 6) vpos = vec2( 0.0, -H);
        else                   vpos = vec2( 0.0,  0.0);
    }

    // Map 2D position back to 3D: base + vpos.x * axisVec + vpos.y * sideDir.
    vec3 pos3d = a.base + vpos.x * axisVec + vpos.y * sideDir;
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(pos3d, 1.0);

    color_fs    = a.color1;
    pickingId_fs = a.pickingId;
}
