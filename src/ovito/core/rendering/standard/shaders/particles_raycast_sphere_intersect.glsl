// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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
