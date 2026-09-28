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

// Non-instanced mesh: pseudo-color mapping vertex shader.
// The 'color' attribute carries the pseudo-color scalar in the R channel and a
// selection flag (non-zero green = selected face) in the G channel.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;   // color.r = pseudo-color scalar; color.g != 0 → selected face

layout(location = 0) out vec3  normal_vs_fs;
layout(location = 1) out float pseudoValue_fs;   // mapped scalar value in [colorRangeMin, colorRangeMax]
layout(location = 2) out float isSelected_fs;    // 1.0 if selected face, 0.0 otherwise

void main()
{
    vec4 posVS = modelViewMatrix * vec4(position, 1.0);
    gl_Position = clipProjectionMatrix * posVS;
    normal_vs_fs = mat3(modelViewMatrix) * normal;
    pseudoValue_fs = color.r;
    isSelected_fs = color.g;  // non-zero means selected
}
