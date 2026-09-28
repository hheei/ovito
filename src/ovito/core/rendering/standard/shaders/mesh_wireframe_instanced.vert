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

// Instanced wireframe: thin 1px lines vertex shader.
// Each mesh instance renders all wireframe edges transformed by its instance TM.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"

// Per-vertex wireframe endpoint.
layout(location = 0) in vec3 position;

// Per-instance transformation matrix rows.
layout(location = 1) in vec4 instTMRow0;
layout(location = 2) in vec4 instTMRow1;
layout(location = 3) in vec4 instTMRow2;

void main()
{
    vec4 posH = vec4(position, 1.0);
    vec3 worldPos = vec3(dot(instTMRow0, posH), dot(instTMRow1, posH), dot(instTMRow2, posH));
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(worldPos, 1.0);
}
