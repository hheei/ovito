// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Geometry snippet: oriented bounding box for superquadric raycast.
// 14 vertices per instance (TriangleStrip). Outputs the view→particle inverse matrix
// and exponents needed by the superquadric fragment intersection algorithm.
// Output varyings match surface/superquadric_raycast.glsl inputs.

#include "../shape_orientation.glsl"

layout(location = 0)  flat out vec4 color_fs;
layout(location = 1)  flat out mat3 view_particle_matrix_fs;  // locations 1,2,3
layout(location = 4)  flat out vec3 particle_view_pos_fs;
layout(location = 5)  flat out vec2 particle_exponents_fs;
layout(location = 6)  noperspective out vec3 ray_origin_fs;
layout(location = 7)  out vec3 ray_dir_fs;
layout(location = 8)  flat out uint instanceIndex_fs;

const vec3 CUBE_VERTS[14] = vec3[14](
    vec3( 1.0,  1.0,  1.0), vec3( 1.0, -1.0,  1.0), vec3( 1.0,  1.0, -1.0),
    vec3( 1.0, -1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0), vec3( 1.0,  1.0,  1.0), vec3(-1.0,  1.0,  1.0),
    vec3( 1.0,  1.0, -1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0),
    vec3(-1.0,  1.0,  1.0), vec3(-1.0, -1.0,  1.0)
);

void emitVertex(ParticleAttribs a, int corner)
{
    mat3 SO = calcShapeOrientation(a.orientation, a.principalAxes, a.radius);

    particle_view_pos_fs = (modelViewMatrix * vec4(a.position, 1.0)).xyz;

    // view_particle_matrix transforms view-space vectors into the unit-particle space.
    view_particle_matrix_fs = inverse(mat3(modelViewMatrix) * SO);

    // Exponents derived from roundness: 2/e and 2/n in the superellipsoid equation.
    particle_exponents_fs.x = 2.0 / (a.roundness.x > 0.0 ? a.roundness.x : 1.0);
    particle_exponents_fs.y = 2.0 / (a.roundness.y > 0.0 ? a.roundness.y : 1.0);

    vec3 worldPos = a.position + SO * CUBE_VERTS[corner];
    vec3 viewPos  = (modelViewMatrix * vec4(worldPos, 1.0)).xyz;
    gl_Position = clipProjectionMatrix * vec4(viewPos, 1.0);

    if(isPerspective != 0) {
        ray_origin_fs = vec3(0.0);
        ray_dir_fs    = viewPos;
    }
    else {
        ray_origin_fs = vec3(viewPos.xy, 0.0);
        ray_dir_fs    = vec3(0.0, 0.0, -1.0);
    }

    color_fs = a.color;
    instanceIndex_fs = a.pickingId;
}
