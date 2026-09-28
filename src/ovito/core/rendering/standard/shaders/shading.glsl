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

// Shading constants and function used by all surface fragment shaders.
#ifndef SHADING_GLSL
#define SHADING_GLSL

const float ambient = 0.4;
const float diffuse_strength = 1.0 - ambient;
const float shininess = 6.0;
const vec3 specular_lightdir = normalize(vec3(-1.8, 1.5, -0.2));

// Computes the shaded fragment color.
// surface_normal: view-space surface normal (need not be front-facing)
// ray_dir: view-space ray direction pointing into the scene (toward -z for orthographic)
vec4 shadedColor(in vec4 color, in vec3 surface_normal, in vec3 ray_dir)
{
    float specular = pow(max(0.0, dot(reflect(specular_lightdir, surface_normal), ray_dir)), shininess) * 0.25;
    float diffuse = abs(surface_normal.z) * diffuse_strength;
    return vec4(color.rgb * (diffuse + ambient) + vec3(specular), color.a);
}

#endif // SHADING_GLSL
