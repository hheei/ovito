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

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "lines_draw_params.glsl"

// Per-vertex position input.
layout(location = 0) in vec3 vertexPosition;

// Pass vertex index to fragment shader to compute line segment index.
layout(location = 0) flat out uint vertexIndex_fs;

void main()
{
    // Apply model-view-projection matrix (identity for pre-projected NDC coordinates).
    gl_Position = clipSpaceCorrMatrix * modelViewProjectionMatrix * vec4(vertexPosition, 1.0);

    // Each line segment uses 2 consecutive vertices; the fragment shader divides by 2 to get the segment index.
    vertexIndex_fs = uint(gl_VertexIndex);
}
