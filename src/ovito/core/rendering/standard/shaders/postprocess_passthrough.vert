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

// Post-process passthrough vertex shader.
// Generates a fullscreen triangle from three vertices (no vertex buffer needed).

#version 450

layout(std140, binding = 0) uniform PostProcessParams {
    int isYUpInNDC;
    int _pad0;
    int _pad1;
    int _pad2;
};

layout(location = 0) out vec2 uv_fs;

void main()
{
    // Generate a fullscreen triangle from vertex IDs 0, 1, 2.
    vec2 pos = vec2(
        float((gl_VertexIndex & 1) != 0) * 4.0 - 1.0,
        float((gl_VertexIndex & 2) != 0) * 4.0 - 1.0
    );

    // UV coordinates in [0,1] range.
    uv_fs = pos * 0.5 + 0.5;

    // Account for Y-up/Y-down differences in NDC vs framebuffer.
    if(isYUpInNDC == 0)
        pos.y = -pos.y;

    gl_Position = vec4(pos, 0.0, 1.0);
}
