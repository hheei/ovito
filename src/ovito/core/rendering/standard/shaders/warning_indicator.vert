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

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform IndicatorParams {
    float x;                // NDC x of left edge of icon quad
    float y_top;            // NDC y of top edge (pre-computed for the current API's Y convention)
    float w;                // NDC width (positive)
    float y_bot;            // NDC y of bottom edge (pre-computed for the current API's Y convention)
    int isYUpInFramebuffer; // 1 = OpenGL (v=0 at bottom), 0 = Vulkan/D3D/Metal (v=0 at top)
    int _pad0;
    int _pad1;
    int _pad2;
};

void main()
{
    float px = x + w * float(gl_VertexIndex & 1);
    // y_top and y_bot are pre-computed by the CPU for the correct NDC convention.
    // Vertex order: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right (triangle strip).
    float py = (gl_VertexIndex >> 1) == 0 ? y_top : y_bot;
    gl_Position = vec4(px, py, 0.0, 1.0);

    // Texture coordinate: u=0..1 left-to-right, v=0..1 top-to-bottom.
    // For y-up framebuffer (OpenGL): OpenGL textures have v=0 at the bottom, so flip v.
    // For y-down framebuffer (Vulkan/D3D/Metal): v=0 is naturally at the top, no flip.
    float u = float(gl_VertexIndex & 1);
    float v = float(gl_VertexIndex >> 1);
    if(isYUpInFramebuffer != 0)
        v = 1.0 - v;
    v_texcoord = vec2(u, v);
}
