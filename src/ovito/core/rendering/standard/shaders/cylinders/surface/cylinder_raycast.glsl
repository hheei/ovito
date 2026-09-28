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

// Surface snippet: ray-cylinder intersection for NormalShading cylinders.
// Input varyings must match geometry/cylinder_raycast_bbox.glsl outputs.

#include "../../shading.glsl"

layout(location = 0) flat in vec4  color1_fs;
layout(location = 1) flat in vec4  color2_fs;
layout(location = 2) flat in vec3  cylinderViewBase_fs;
layout(location = 3) flat in vec3  cylinderViewAxis_fs;
layout(location = 4) flat in float cylinderRadiusSq_fs;
layout(location = 5) flat in float cylinderLength_fs;
layout(location = 6) flat in uint  pickingId_fs;

// The rasterized geometry is a back-face-culled bounding box that fully encloses
// the cylinder, so the analytic surface depth is always ≥ the front-face depth.
// depth_greater re-enables HiZ for Vulkan/Metal; the HLSL variant is patched at
// build time to plain SV_Depth (SPIRV-Cross omits noperspective_centroid on SV_Position
// when SV_DepthGreaterEqual is output, failing DXIL validation on Direct3D).
#ifndef OVITO_DISABLE_DEPTH_GREATER
layout(depth_greater) out float gl_FragDepth;
#else
out float gl_FragDepth;
#endif

SurfaceSample sampleSurface()
{
    // Reconstruct ray in view space.
    vec3 rayOrigin, ray_dir_norm;
    if(isPerspective != 0) {
        rayOrigin = vec3(0.0);
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
        rayOrigin  = vec3(x, y, 0.0);
        ray_dir_norm = vec3(0.0, 0.0, -1.0);
    }

    // Correct for Y-flip if needed.
    if(isYUpInFramebuffer == 0) {
        rayOrigin.y = -rayOrigin.y;
        ray_dir_norm.y    = -ray_dir_norm.y;
    }

    // Ray-cylinder intersection, solved via the closest approach of the ray to
    // the cylinder axis. (The previous formulation, ported from the legacy
    // OpenGL cylinder shader, computed a quadratic discriminant as
    // dot(rcPerp,rcPerp) - radius^2, i.e. as the difference of two terms whose
    // magnitude scales with the *distance from the camera to the cylinder axis*
    // rather than with the cylinder's radius. For a thin cylinder viewed from a
    // distance much larger than its radius, those two terms nearly cancel,
    // wiping out several decimal digits of precision — visible as spurious
    // per-pixel dropout on software Vulkan rasterizers such as llvmpipe, whose
    // floating-point rounding differs slightly from typical hardware GPU
    // drivers. Computing the perpendicular offset at the closest-approach point
    // directly (closestPerp below) avoids ever forming that large cancelling
    // difference.)
    vec3 uAxis = cylinderViewAxis_fs / cylinderLength_fs;
    vec3 RC = rayOrigin - cylinderViewBase_fs;

    vec3  dPerp = ray_dir_norm - dot(ray_dir_norm, uAxis) * uAxis;
    float a     = dot(dPerp, dPerp);

    vec3  viewHit;
    vec3  surfNorm;
    float x_param = 0.0;  // Position along cylinder [0..1] for color interpolation.

    if(a < 1e-14) {
        // Ray is parallel to cylinder axis.
        float t = dot(RC, ray_dir_norm);
        float v = dot(RC, RC);
        if(v - t*t > cylinderRadiusSq_fs) discard;

        viewHit  = rayOrigin - t * ray_dir_norm;
        surfNorm = -uAxis;
        float tfar = dot(cylinderViewAxis_fs, ray_dir_norm);
        if(tfar < 0.0 && singleCylinderCap == 0) {
            viewHit += tfar * ray_dir_norm;
            surfNorm = uAxis;
            x_param  = 1.0;
        } else {
            x_param = 0.0;
        }
    } else {
        vec3  rcPerp      = RC - dot(RC, uAxis) * uAxis;
        float tca         = -dot(rcPerp, dPerp) / a;
        vec3  closestPerp = rcPerp + tca * dPerp;
        float dSq         = dot(closestPerp, closestPerp);
        if(dSq > cylinderRadiusSq_fs) discard;

        float thc   = sqrt((cylinderRadiusSq_fs - dSq) / a);
        float tnear = tca - thc;
        float tfar  = tca + thc;

        vec3  hitNear = rayOrigin + tnear * ray_dir_norm;
        float aNear   = dot(hitNear - cylinderViewBase_fs, uAxis) / cylinderLength_fs;

        if(aNear >= 0.0 && aNear <= 1.0) {
            viewHit  = hitNear;
            surfNorm = hitNear - (cylinderViewBase_fs + aNear * cylinderViewAxis_fs);
            x_param  = aNear;
        } else {
            vec3  hitFar = rayOrigin + tfar * ray_dir_norm;
            float aFar   = dot(hitFar - cylinderViewBase_fs, uAxis) / cylinderLength_fs;

            if(aNear < 0.0 && aFar > 0.0) {
                // Base cap.
                viewHit  = rayOrigin + mix(tnear, tfar, aNear / (aNear - aFar)) * ray_dir_norm;
                surfNorm = -uAxis;
                x_param  = 0.0;
            } else if(aNear > 1.0 && aFar < 1.0 && singleCylinderCap == 0) {
                // Head cap.
                viewHit  = rayOrigin + mix(tnear, tfar, (aNear - 1.0) / (aNear - aFar)) * ray_dir_norm;
                surfNorm = uAxis;
                x_param  = 1.0;
            } else {
                discard;
            }
        }
    }

    // Compute NDC depth from the view-space intersection point.
    vec4 projected = projectionMatrix * vec4(viewHit, 1.0);
    float zdepth   = (projected.z / projected.w) * 0.5 + 0.5;

    SurfaceSample s;
    s.viewPos   = viewHit;
    s.depth     = zdepth;
    s.normal    = normalize(surfNorm);
    s.rayDir    = ray_dir_norm;
    s.color     = mix(color1_fs, color2_fs, x_param);
    s.pickingId = pickingId_fs;
    return s;
}
