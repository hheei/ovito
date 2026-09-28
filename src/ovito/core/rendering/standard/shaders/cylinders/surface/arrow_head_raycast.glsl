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

// Surface snippet: ray-cone intersection for arrow head (NormalShading arrows).
// Input varyings must match geometry/arrow_head_raycast_bbox.glsl outputs.

#include "../../shading.glsl"

layout(location = 0) flat in vec4  arrowColor_fs;
layout(location = 1) flat in vec3  coneCenter_fs;
layout(location = 2) flat in vec3  coneAxis_fs;
layout(location = 3) flat in float coneRadius_fs;
layout(location = 4) flat in uint  pickingId_fs;

// The rasterized geometry is a back-face-culled bounding box that fully encloses
// the cone, so the analytic surface depth is always ≥ the front-face depth.
// depth_greater re-enables HiZ for Vulkan/Metal; the HLSL variant is patched at
// build time to plain SV_Depth (SPIRV-Cross omits noperspective_centroid on SV_Position
// when SV_DepthGreaterEqual is output, failing DXIL validation on Direct3D).
#ifndef OVITO_DISABLE_DEPTH_GREATER
layout(depth_greater) out float gl_FragDepth;
#else
out float gl_FragDepth;
#endif

const float CONE_RATIO = 1.8;

SurfaceSample sampleSurface()
{
    // Reconstruct the ray directly from gl_FragCoord, exactly as cylinder_raycast.glsl
    // does, rather than through a noperspective-interpolated varying computed once per
    // box corner in the vertex shader. For a thin, elongated arrow head the box's side
    // quads rasterize as extremely thin sliver triangles; interpolating the ray origin
    // and direction across such an ill-conditioned triangle introduces errors far larger
    // than reconstructing them analytically per fragment. This was confirmed to be the
    // source of spurious interior pixel dropout in the (structurally identical) arrow
    // tail shader on software Vulkan rasterizers such as llvmpipe (see
    // arrow_tail_raycast.glsl for the diagnosis).
    vec3 ray_origin, ray_dir_norm;
    if(isPerspective != 0) {
        ray_origin = vec3(0.0);
        ray_dir_norm = normalize(vec3(
            (gl_FragCoord.x / viewportWidth  * 2.0 - 1.0) / projectionMatrix[0][0],
            (gl_FragCoord.y / viewportHeight * 2.0 - 1.0) / projectionMatrix[1][1],
            -1.0
        ));
    } else {
        float ndcX = gl_FragCoord.x / viewportWidth  * 2.0 - 1.0;
        float ndcY = gl_FragCoord.y / viewportHeight * 2.0 - 1.0;
        float x    = ndcX / projectionMatrix[0][0] - projectionMatrix[3][0] / projectionMatrix[0][0];
        float y    = ndcY / projectionMatrix[1][1] - projectionMatrix[3][1] / projectionMatrix[1][1];
        ray_origin = vec3(x, y, 0.0);
        ray_dir_norm = vec3(0.0, 0.0, -1.0);
    }
    if(isYUpInFramebuffer == 0) {
        ray_origin.y = -ray_origin.y;
        ray_dir_norm.y = -ray_dir_norm.y;
    }

    float heightSq = dot(coneAxis_fs, coneAxis_fs);

    // Numeric precision: for orthographic, shift ray origin toward cone so that E
    // (below) stays small regardless of how far the object is from the arbitrary
    // z=0 reference plane used for the orthographic ray origin. Orthographic rays
    // have no meaningful "behind the camera" half-space (unlike perspective, where
    // t must be positive), so zmin is just a permissive sentinel here; the ddot/
    // heightSq checks already restrict hits to the correct nappe of the cone.
    float zmin;
    vec3  rayOriginShifted = ray_origin;
    if(isPerspective == 0) {
        zmin = -1.0e30;
        rayOriginShifted.z = coneCenter_fs.z;
    } else {
        zmin = 0.0;
    }

    // Cone quadric intersection (ported from legacy arrow_head.frag).
    const float coneAngle      = atan(1.0 / CONE_RATIO);
    const float coneCosSquared = cos(coneAngle) * cos(coneAngle);

    vec3  axisNorm = normalize(coneAxis_fs);
    float AdD = dot(axisNorm, ray_dir_norm);
    vec3  E   = rayOriginShifted - coneCenter_fs;
    float AdE = dot(axisNorm, E);
    float DdE = dot(ray_dir_norm, E);
    float EdE = dot(E, E);
    float c2  = AdD * AdD - coneCosSquared;
    float c1  = AdD * AdE - coneCosSquared * DdE;
    float c0  = AdE * AdE - coneCosSquared * EdE;

    float ray_t  = zmin;
    float epsilon = 1e-9 * coneRadius_fs * coneRadius_fs;
    vec3  Ehit = E;

    if(abs(c2) >= epsilon) {
        float discr = c1 * c1 - c0 * c2;
        if(discr < -epsilon) {
            // Q(t) = 0 has no real-valued roots: ray misses the cone.
            // ray_t stays at zmin → the single final discard below handles it.
            // (Early discard removed: multiple OpKill in one function triggers a
            //  spirv-cross HLSL emitter bug, "argument pulled into unrelated predicate".)
        }
        else if(discr > epsilon) {
            // Q(t) = 0 has two distinct real-valued roots.  However, one or
			// both of them might intersect the portion of the double-sided
			// cone "behind" the vertex.  We are interested only in those
			// intersections "in front" of the vertex.
            float root = sqrt(discr);
            float t    = (-c1 - root) / c2;
            vec3  Et   = rayOriginShifted + t * ray_dir_norm - coneCenter_fs;
            float ddot = dot(Et, coneAxis_fs);
            if(ddot > 0.0 && ddot < heightSq && t > zmin) {
                ray_t = t;
                Ehit = Et;
            }
            t  = (-c1 + root) / c2;
            Et = rayOriginShifted + t * ray_dir_norm - coneCenter_fs;
            ddot = dot(Et, coneAxis_fs);
            if(ddot > 0.0 && ddot < heightSq && t > zmin) {
                ray_t = t;
                Ehit = Et;
            }
        } else {
            // One repeated real root (line is tangent to the cone).
            float t = -(c1 / c2);
            Ehit = rayOriginShifted + t * ray_dir_norm - coneCenter_fs;
            if(dot(Ehit, coneAxis_fs) > 0.0) {
                ray_t = t;
            }
        }
    } else if(abs(c1) >= epsilon) {
        // c2 = 0, c1 != 0 (D is a direction vector on the cone boundary)
        float t = -(0.5 * c0 / c1);
        Ehit = rayOriginShifted + t * ray_dir_norm - coneCenter_fs;
        if(dot(Ehit, coneAxis_fs) > 0.0) {
            ray_t = t;
        }
    } else if(abs(c0) >= epsilon) {
        // c2 = c1 = 0, c0 ≠ 0: Q is a non-zero constant, no intersection.
        // ray_t stays at zmin → the single final discard below handles it.
    } else if(DdE > 0.0) {
        // c2 = c1 = c0 = 0, cone contains ray V+t*D where V is cone vertex
		// and D is the line direction.
        ray_t = DdE;
    }
    if(ray_t <= zmin) {
        discard;
    }

    vec3 viewHit  = Ehit + coneCenter_fs;
    vec3 surfNorm = cross(Ehit, cross(Ehit, coneAxis_fs));

    // Check disc (base of cone).
    vec3  discCenter = coneCenter_fs + coneAxis_fs;
    vec3  discNormal = coneAxis_fs;
    float denom      = dot(discNormal, ray_dir_norm);
    if(abs(denom) > 1e-8) {
        float td = -(dot(discNormal, rayOriginShifted) + dot(discNormal, -discCenter)) / denom;
        if(td > zmin && td < ray_t) {
            vec3 hitPnt = rayOriginShifted + td * ray_dir_norm - discCenter;
            if(dot(hitPnt, hitPnt) < coneRadius_fs * coneRadius_fs) {
                viewHit  = rayOriginShifted + td * ray_dir_norm;
                surfNorm = discNormal;
            }
        }
    }

    vec4  projected = projectionMatrix * vec4(viewHit, 1.0);
    float zdepth    = (projected.z / projected.w) * 0.5 + 0.5;

    SurfaceSample s;
    s.viewPos   = viewHit;
    s.depth     = zdepth;
    s.normal    = normalize(surfNorm);
    s.rayDir    = ray_dir_norm;
    s.color     = arrowColor_fs;
    s.pickingId = pickingId_fs;
    return s;
}
