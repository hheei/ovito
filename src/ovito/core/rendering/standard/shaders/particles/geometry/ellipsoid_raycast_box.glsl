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

// Geometry snippet: oriented bounding box for ellipsoid raycast.
// 14 vertices per instance (TriangleStrip). Outputs matrices needed by the ellipsoid
// fragment intersection algorithm.
// Output varyings match surface/ellipsoid_raycast.glsl inputs.

#include "../shape_orientation.glsl"

layout(location = 0)  flat out vec4 color_fs;
layout(location = 1)  flat out mat3 view_to_sphere_fs;   // locations 1,2,3
layout(location = 4)  flat out mat3 sphere_to_view_fs;   // locations 4,5,6
layout(location = 7)  flat out vec3 particle_view_pos_fs;
layout(location = 8)  noperspective out vec3 ray_origin_fs;
layout(location = 9)  out vec3 ray_dir_fs;
layout(location = 10) flat out uint instanceIndex_fs;

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

    // Particle center in view space.
    particle_view_pos_fs = (modelViewMatrix * vec4(a.position, 1.0)).xyz;

    // sphere_to_view transforms from unit-sphere local space to view space.
    sphere_to_view_fs = mat3(modelViewMatrix) * SO;
    // view_to_sphere is its inverse (transforms view rays into unit-sphere space).
    view_to_sphere_fs = inverse(sphere_to_view_fs);

    // World-space vertex position on the bounding box.
    vec3 worldPos = a.position + SO * CUBE_VERTS[corner];
    vec3 viewPos  = (modelViewMatrix * vec4(worldPos, 1.0)).xyz;
    gl_Position = clipProjectionMatrix * vec4(viewPos, 1.0);

    // View ray through this vertex (interpolated per-fragment via noperspective).
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
