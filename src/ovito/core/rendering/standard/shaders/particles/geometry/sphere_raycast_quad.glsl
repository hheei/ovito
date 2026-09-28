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

// Geometry snippet: raycast sphere billboard quad.
// Emits a tight bounding quad for ray-sphere intersection in the fragment shader.
// For perspective, uses the tangent-envelope approach; for orthographic, a simple scaled quad.
// Output varyings must match the input declarations in surface/sphere_raycast.glsl.

layout(location = 0) flat out vec4 color_fs;
layout(location = 1) flat out vec3 particle_view_pos_fs;
layout(location = 2) flat out float particle_radius_squared_fs;
layout(location = 3) out vec3 ray_origin_fs;
layout(location = 4) out vec3 ray_dir_fs;
layout(location = 5) flat out uint instanceIndex_fs;

void emitVertex(ParticleAttribs a, int corner)
{
    particle_view_pos_fs = (modelViewMatrix * vec4(a.position, 1.0)).xyz;

    float particle_radius = a.radius * uniformModelScale;
    particle_radius_squared_fs = particle_radius * particle_radius;

    // Merge both isPerspective branches into one to avoid the spirv-cross
    // "argument pulled into unrelated predicate" HLSL decompilation error that
    // is triggered when the same uniform condition appears twice in one function.
    if(isPerspective != 0) {
        vec3 sphere_dir = particle_view_pos_fs;
        float sphere_dist_sq = dot(sphere_dir, sphere_dir);
        float sphere_dist = sqrt(sphere_dist_sq);
        float tangent_dist = sqrt(sphere_dist_sq - particle_radius_squared_fs);
        float alpha = acos(tangent_dist / sphere_dist);
        vec3 dir = cross(sphere_dir, vec3(0.0, 1.0, 0.0));
        float scaling = sphere_dist * tan(alpha) * sqrt(2.0);
        vec3 uv;
        if(corner == 0) uv = scaling * normalize(dir);
        else if(corner == 1) uv = scaling * normalize(cross(dir, sphere_dir));
        else if(corner == 2) uv = -scaling * normalize(cross(dir, sphere_dir));
        else uv = -scaling * normalize(dir);
        vec3 vertex_view_pos = particle_view_pos_fs + uv;

        // Shift the vertex along the camera ray onto the sphere's front-cap plane,
        // so the rasterized depth equals the closest point of the surface along
        // the ray from the camera origin. Scaling along the ray preserves NDC xy
        // (both points project to the same screen position) and overrides only
        // NDC z. After the shift, the analytic surface depth is always ≥ the
        // rasterized depth, which makes layout(depth_greater) in the fragment
        // shader safe and re-enables HiZ / early-Z.
        float frontZ = particle_view_pos_fs.z * (1.0 - particle_radius / sphere_dist);
        vertex_view_pos *= frontZ / vertex_view_pos.z;

        gl_Position = clipProjectionMatrix * vec4(vertex_view_pos, 1.0);
        ray_origin_fs = vec3(0.0);
        ray_dir_fs = vertex_view_pos;  // still on the same camera ray, so direction is preserved
    }
    else {
        float scaling = particle_radius * sqrt(2.0);
        vec3 uv;
        if(corner == 0) uv = vec3( scaling, 0.0, 0.0);
        else if(corner == 1) uv = vec3(0.0,  scaling, 0.0);
        else if(corner == 2) uv = vec3(0.0, -scaling, 0.0);
        else uv = vec3(-scaling, 0.0, 0.0);
        vec3 vertex_view_pos = particle_view_pos_fs + uv;

        // Shift the quad forward to the sphere's front-cap plane. Under
        // orthographic projection the camera rays are parallel to the view axis,
        // so adjusting only eye-z preserves NDC xy. After the shift the analytic
        // surface depth is always ≥ the rasterized depth → layout(depth_greater).
        vertex_view_pos.z += particle_radius;

        gl_Position = clipProjectionMatrix * vec4(vertex_view_pos, 1.0);
        ray_origin_fs = vec3(vertex_view_pos.xy, 0.0);
        ray_dir_fs = vec3(0.0, 0.0, -1.0);
    }

    color_fs = a.color;
    instanceIndex_fs = a.pickingId;
}
