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

// OIT composite vertex shader.
// Generates a fullscreen triangle from three vertices (no vertex buffer needed).

#version 450
#extension GL_GOOGLE_include_directive : enable

#include "scene_params.glsl"

layout(location = 0) out vec2 uv_fs;

void main()
{
    // Generate a fullscreen triangle from vertex IDs 0, 1, 2.
    // Triangle covers the full NDC range [-1, 1] with a 2x2 quad approach.
    vec2 pos = vec2(
        float((gl_VertexIndex & 1) != 0) * 4.0 - 1.0,
        float((gl_VertexIndex & 2) != 0) * 4.0 - 1.0
    );

    // UV coordinates in [0,1] range.
    uv_fs = pos * 0.5 + 0.5;

    // Metal has Y-up NDC but Y-down framebuffer storage; flip UV.y to compensate.
    // On D3D/Vulkan both conventions match; on OpenGL both are Y-up — no flip in either case.
    if(isYUpInNDC != isYUpInFramebuffer)
        uv_fs.y = 1.0 - uv_fs.y;

    // No Y-flip of gl_Position needed: the fullscreen triangle covers the viewport
    // identically in all backends, regardless of NDC Y direction.
    gl_Position = vec4(pos, 0.0, 1.0);
}
