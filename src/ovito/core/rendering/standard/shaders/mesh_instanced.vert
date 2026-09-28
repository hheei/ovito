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

// Instanced mesh: visual pass vertex shader (no per-instance colors).
// Per-vertex face color from the mesh is used directly.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-vertex mesh data.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

// Per-instance transformation matrix rows (3 × vec4 = affine 3×4 matrix).
layout(location = 3) in vec4 instTMRow0;
layout(location = 4) in vec4 instTMRow1;
layout(location = 5) in vec4 instTMRow2;

layout(location = 0) out vec4 color_fs;
layout(location = 1) out vec3 normal_vs_fs;

void main()
{
    // Apply per-instance affine transform (3×4 row-major, last row is implicit [0,0,0,1]).
    vec4 posH = vec4(position, 1.0);
    vec3 worldPos = vec3(dot(instTMRow0, posH), dot(instTMRow1, posH), dot(instTMRow2, posH));
    vec4 posVS = modelViewMatrix * vec4(worldPos, 1.0);
    gl_Position = clipProjectionMatrix * posVS;

    // Transform normal: assume affine transform has no non-uniform scale.
    vec4 normalH = vec4(normal, 0.0);
    vec3 worldNormal = vec3(dot(instTMRow0, normalH), dot(instTMRow1, normalH), dot(instTMRow2, normalH));
    normal_vs_fs = mat3(modelViewMatrix) * worldNormal;

    color_fs = color;
}
