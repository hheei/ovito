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

// Ray-sphere intersection. Declares ray_dir_norm and view_intersection_pnt.
// Calls discard on miss or behind-camera hits.
vec3 ray_dir_norm = normalize(ray_dir_fs);
vec3 sphere_dir = particle_view_pos_fs - ray_origin_fs;

float b = dot(ray_dir_norm, sphere_dir);
vec3 delta = ray_dir_norm * b - sphere_dir;
float x = dot(delta, delta);
float disc = particle_radius_squared_fs - x;

if(disc < 0.0)
    discard;

float tnear = b - sqrt(disc);

// For orthographic projection tnear < 0 is valid (znear can be negative).
// Only discard for perspective projection.
if(isPerspective != 0 && tnear < 0.0)
    discard;

vec3 view_intersection_pnt = ray_origin_fs + tnear * ray_dir_norm;
