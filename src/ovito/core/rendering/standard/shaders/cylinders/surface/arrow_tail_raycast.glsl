// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Surface snippet: ray-cylinder intersection for arrow tail (NormalShading arrows).
// Same algorithm as cylinder_raycast.glsl but with singleCylinderCap=1 (only base cap).
// Input varyings must match geometry/arrow_tail_raycast_bbox.glsl outputs.

#include "../../shading.glsl"

layout(location = 0) flat in vec4  arrowColor_fs;
layout(location = 1) flat in vec3  cylinderViewBase_fs;
layout(location = 2) flat in vec3  cylinderViewAxis_fs;
layout(location = 3) flat in float cylinderRadiusSq_fs;
layout(location = 4) flat in float cylinderLength_fs;
layout(location = 5) flat in uint  pickingId_fs;

// The rasterized geometry is a back-face-culled bounding box that fully encloses
// the cylinder tail, so the analytic surface depth is always ≥ the front-face depth.
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
    // Reconstruct the ray directly from gl_FragCoord, exactly as cylinder_raycast.glsl
    // does, rather than through a noperspective-interpolated varying computed once per
    // box corner in the vertex shader. For a thin, elongated arrow shaft the box's side
    // quads rasterize as extremely thin sliver triangles; interpolating the ray origin
    // and direction across such an ill-conditioned triangle introduces errors far larger
    // than reconstructing them analytically per fragment, and was the actual source of
    // spurious interior pixel dropout on software Vulkan rasterizers such as llvmpipe
    // (confirmed by comparing against this exact reconstruction, which eliminates it).
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
    if(isYUpInFramebuffer == 0) {
        rayOrigin.y = -rayOrigin.y;
        ray_dir_norm.y = -ray_dir_norm.y;
    }

    vec3 uAxis = cylinderViewAxis_fs / cylinderLength_fs;
    vec3 RC    = rayOrigin - cylinderViewBase_fs;

    // Decompose the ray direction into components parallel/perpendicular to the
    // cylinder axis and solve the infinite-cylinder intersection via the closest
    // approach of the ray to the axis. The previous formulation (ported from the
    // legacy OpenGL cylinder shader) computed a quadratic discriminant as
    // dot(rcPerp,rcPerp) - radius^2, i.e. as the difference of two terms whose
    // magnitude scales with the *distance from the camera to the cylinder axis*
    // rather than with the cylinder's radius. For a thin cylinder viewed from a
    // distance much larger than its radius, those two terms nearly cancel,
    // wiping out several decimal digits of precision. Computing the perpendicular
    // offset at the closest-approach point directly (closestPerp below) avoids
    // ever forming that large cancelling difference.
    vec3  dPerp = ray_dir_norm - dot(ray_dir_norm, uAxis) * uAxis;
    float a     = dot(dPerp, dPerp);

    vec3  viewHit;
    vec3  surfNorm;

    if(a < 1e-14) {
        float t = dot(RC, ray_dir_norm);
        float v = dot(RC, RC);
        if(v - t*t > cylinderRadiusSq_fs) discard;
        viewHit  = rayOrigin - t * ray_dir_norm;
        surfNorm = -uAxis;
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
        } else {
            vec3  hitFar = rayOrigin + tfar * ray_dir_norm;
            float aFar   = dot(hitFar - cylinderViewBase_fs, uAxis) / cylinderLength_fs;

            if(aNear < 0.0 && aFar > 0.0) {
                viewHit  = rayOrigin + mix(tnear, tfar, aNear / (aNear - aFar)) * ray_dir_norm;
                surfNorm = -uAxis;
            } else {
                discard;
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
