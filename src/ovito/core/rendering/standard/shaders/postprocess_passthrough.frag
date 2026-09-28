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

// Post-process passthrough fragment shader.
// Samples the intermediate scene color texture and outputs it unchanged.
// This is the base of the post-processing pipeline; outline/highlight effects
// will replace this shader with a more complex one.

#version 450

layout(binding = 1) uniform sampler2D colorTex;  // Intermediate scene color (RGBA8).

layout(location = 0) in vec2 uv_fs;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(colorTex, uv_fs);
}
