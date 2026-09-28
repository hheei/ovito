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

// Non-instanced mesh: pseudo-color mapping fragment shader.
// Samples a 1D color map texture and applies Phong shading.
// Selected faces are highlighted with the selection color.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "mesh_draw_params.glsl"
#include "shading.glsl"

layout(binding = 2) uniform sampler2D colorMapTex;

layout(location = 0) in vec3  normal_vs_fs;
layout(location = 1) in float pseudoValue_fs;
layout(location = 2) in float isSelected_fs;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 baseColor;
    if(isSelected_fs != 0.0) {
        // Selected face: use selection highlight color.
        baseColor = selectionColor;
    } else {
        // Map pseudo-color scalar to texture color.
        float t = (pseudoValue_fs - colorRangeMin) / max(colorRangeMax - colorRangeMin, 1.0e-10);
        t = clamp(t, 0.0, 1.0);
        baseColor = texture(colorMapTex, vec2(t, 0.5));
    }
    baseColor.a *= opacity;
    vec3 ray_dir;
    if(isPerspective != 0) {
        float ndcX = gl_FragCoord.x / viewportWidth  * 2.0 - 1.0;
        float ndcY = gl_FragCoord.y / viewportHeight * 2.0 - 1.0;
        if(isYUpInFramebuffer == 0) ndcY = -ndcY;
        ray_dir = normalize(vec3(ndcX / projectionMatrix[0][0], ndcY / projectionMatrix[1][1], -1.0));
    } else {
        ray_dir = vec3(0.0, 0.0, -1.0);
    }
    fragColor = shadedColor(baseColor, normalize(normal_vs_fs), ray_dir);
}
