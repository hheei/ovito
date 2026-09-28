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

layout(std140, binding = 0) uniform ImageParams {
    vec4 ndcRect;               // (x_min, y_min, x_max, y_max) in NDC
    int isYUpInNDC;
    int _pad0; int _pad1; int _pad2;
};

void main()
{
    // Triangle strip: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right.
    float u = float(gl_VertexIndex & 1);
    float v = float(gl_VertexIndex >> 1);

    float px = mix(ndcRect.x, ndcRect.z, u);
    float py = mix(ndcRect.y, ndcRect.w, v);
    gl_Position = vec4(px, py, 0.0, 1.0);

    // Texture coordinate: u=0..1 left-to-right, v=0..1 top-to-bottom in QImage.
    // For y-up NDC backends (OpenGL, Metal): v=0 maps to screen bottom, so we must flip tv
    // so that the top of the image (texture v=0) appears at the top of the screen (NDC y_max).
    // For y-down NDC backends (Vulkan): v=0 already maps to screen top, no flip needed.
    float tv = v;
    if(isYUpInNDC != 0)
        tv = 1.0 - tv;
    v_texcoord = vec2(u, tv);
}
