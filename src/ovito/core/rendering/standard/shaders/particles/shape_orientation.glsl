// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Utility: compute a combined rotation+scale matrix from an orientation quaternion and aspherical
// shape axes. Returns a column-major mat3 where each column is a scaled principal axis in the
// particle's local frame. If asphericalAxes is zero, uses uniform radius scaling.

#ifndef SHAPE_ORIENTATION_GLSL
#define SHAPE_ORIENTATION_GLSL

mat3 calcShapeOrientation(vec4 quat, vec3 asphericalAxes, float radius)
{
    vec3 axes = (asphericalAxes != vec3(0.0)) ? asphericalAxes : vec3(radius);

    float norm = length(quat);
    vec4 q = (norm > 1e-9) ? quat / norm : vec4(0.0, 0.0, 0.0, 1.0);

    mat3 rot = mat3(
        1.0 - 2.0*(q.y*q.y + q.z*q.z),   2.0*(q.x*q.y + q.w*q.z),   2.0*(q.x*q.z - q.w*q.y),
              2.0*(q.x*q.y - q.w*q.z), 1.0 - 2.0*(q.x*q.x + q.z*q.z),   2.0*(q.y*q.z + q.w*q.x),
              2.0*(q.x*q.z + q.w*q.y),       2.0*(q.y*q.z - q.w*q.x), 1.0 - 2.0*(q.x*q.x + q.y*q.y));
    rot[0] *= axes.x;
    rot[1] *= axes.y;
    rot[2] *= axes.z;
    return rot;
}

#endif // SHAPE_ORIENTATION_GLSL
