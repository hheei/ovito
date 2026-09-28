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

// Instanced mesh: sorted transparency vertex shader.
// Reads instance data from SSBOs so that gl_InstanceIndex can be remapped
// through the sorted-index buffer for back-to-front ordering.
// Per-instance colors are also read from SSBO; if no colors are needed, the
// color SSBO contains the uniform face color repeated N times.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-vertex mesh data (VBO, same for all instances).
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

// SSBOs for sorted instance data.
layout(std430, binding = 3) readonly buffer SortedIndices { uint sortedIndices[]; };
layout(std430, binding = 4) readonly buffer InstanceTMs   { vec4 tmRows[]; };       // 3 rows per instance
layout(std430, binding = 5) readonly buffer InstanceColors { vec4 instColors[]; };  // 1 color per instance

layout(location = 0) out vec4 color_fs;
layout(location = 1) out vec3 normal_vs_fs;

void main()
{
    uint actualInst = sortedIndices[gl_InstanceIndex];
    vec4 row0 = tmRows[actualInst * 3u + 0u];
    vec4 row1 = tmRows[actualInst * 3u + 1u];
    vec4 row2 = tmRows[actualInst * 3u + 2u];
    vec4 instColor = instColors[actualInst];

    vec4 posH = vec4(position, 1.0);
    vec3 worldPos = vec3(dot(row0, posH), dot(row1, posH), dot(row2, posH));
    vec4 posVS = modelViewMatrix * vec4(worldPos, 1.0);
    gl_Position = clipProjectionMatrix * posVS;

    vec4 normalH = vec4(normal, 0.0);
    vec3 worldNormal = vec3(dot(row0, normalH), dot(row1, normalH), dot(row2, normalH));
    normal_vs_fs = mat3(modelViewMatrix) * worldNormal;

    color_fs = vec4(color.rgb * instColor.rgb, color.a * instColor.a);
}
