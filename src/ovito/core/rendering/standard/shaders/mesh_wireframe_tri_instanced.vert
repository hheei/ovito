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

// Instanced wireframe: thick lines rendered as screen-aligned quads.
// Instancing: outer = mesh instances, inner = wireframe edges.
// Combined instance = (meshInstance * edgeCount + edgeIndex).
// The edge VBO is addressed via gl_VertexIndex (four quad corners), while
// gl_InstanceIndex addresses (meshInstance * edgeCount + edgeIndex) for the
// sorted edge→TM look-up. SSBOs hold sorted edge pairs and instance TMs.
//
// For simplicity in the VBO path this shader is only used when instanced thick
// wireframe is needed. The combined instance count = N_meshInstances * N_edges.
//
// See mesh_wireframe_tri.vert for the rationale behind the two expansion paths
// (Bresenham-style parallelogram vs. screen-perpendicular rectangle).

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-(combined-)instance inputs.
// position_from/to are per mesh-edge, TM rows are per mesh-instance.
// We pack them together by expanding the wireframe buffer N_mesh_instances times
// on the CPU, so the stride between mesh-instance TM repeats equals N_edges.
// Both position_from/to and TM rows step PerInstance.
layout(location = 0) in vec3 position_from;
layout(location = 1) in vec3 position_to;
layout(location = 2) in vec4 instTMRow0;
layout(location = 3) in vec4 instTMRow1;
layout(location = 4) in vec4 instTMRow2;

const float BRESENHAM_THRESHOLD = 1.5;

void main()
{
    int corner = gl_VertexIndex;

    // Apply per-instance mesh transform to both endpoints.
    vec4 fromH = vec4(position_from, 1.0);
    vec4 toH   = vec4(position_to,   1.0);
    vec3 worldFrom = vec3(dot(instTMRow0, fromH), dot(instTMRow1, fromH), dot(instTMRow2, fromH));
    vec3 worldTo   = vec3(dot(instTMRow0, toH),   dot(instTMRow1, toH),   dot(instTMRow2, toH));

    vec4 projFrom = clipProjectionMatrix * modelViewMatrix * vec4(worldFrom, 1.0);
    vec4 projTo   = clipProjectionMatrix * modelViewMatrix * vec4(worldTo,   1.0);

    float widthPx = lineThickness * viewportHeight;
    bool crossesCamera = (projTo.w * projFrom.w < 0.0);
    bool useBresenham = (widthPx <= BRESENHAM_THRESHOLD) && !crossesCamera;

    vec4 expandedPos;

    if(useBresenham) {
        vec2 viewport = vec2(viewportWidth, viewportHeight);
        vec2 fromPx = (projFrom.xy / projFrom.w * 0.5 + 0.5) * viewport;
        vec2 toPx   = (projTo.xy   / projTo.w   * 0.5 + 0.5) * viewport;

        if(isYUpInNDC != 0) {
            fromPx.y = viewport.y - fromPx.y;
            toPx.y   = viewport.y - toPx.y;
        }

        fromPx = floor(fromPx) + 0.5;
        toPx   = floor(toPx)   + 0.5;

        vec2 d = toPx - fromPx;
        vec2 offset = (abs(d.x) >= abs(d.y))
            ? vec2(0.0, widthPx * 0.5)
            : vec2(widthPx * 0.5, 0.0);

        vec2 endpointPx = (corner < 2) ? fromPx : toPx;
        float sgn = ((corner & 1) == 0) ? -1.0 : 1.0;
        vec2 cornerPx = endpointPx + sgn * offset;

        if(isYUpInNDC != 0)
            cornerPx.y = viewport.y - cornerPx.y;

        vec2 cornerNDC = cornerPx / viewport * 2.0 - 1.0;
        float wOut = (corner < 2) ? projFrom.w : projTo.w;
        float zOut = (corner < 2) ? projFrom.z : projTo.z;
        expandedPos = vec4(cornerNDC * wOut, zOut, wOut);
    }
    else {
        vec2 delta = normalize(projTo.xy / projTo.w - projFrom.xy / projFrom.w) * lineThickness;

        if(crossesCamera)
            delta = -delta;

        delta.y *= viewportHeight / viewportWidth;

        if(corner == 0)
            expandedPos = projFrom - vec4( delta.y * projFrom.w, -delta.x * projFrom.w, 0.0, 0.0);
        else if(corner == 1)
            expandedPos = projFrom + vec4( delta.y * projFrom.w, -delta.x * projFrom.w, 0.0, 0.0);
        else if(corner == 2)
            expandedPos = projTo   - vec4( delta.y * projTo.w,   -delta.x * projTo.w,   0.0, 0.0);
        else
            expandedPos = projTo   + vec4( delta.y * projTo.w,   -delta.x * projTo.w,   0.0, 0.0);
    }

    gl_Position = expandedPos;
}
