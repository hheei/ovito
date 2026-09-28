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

// Surface snippet: iterative ray-superquadric intersection.
// Ports the secant+bisection algorithm from the legacy OpenGL superquadric fragment shader.
// Input varyings must match the output declarations in geometry/superquadric_raycast_box.glsl.
// Provides: sampleSurface() -> SurfaceSample.

layout(location = 0)  flat in vec4 color_fs;
layout(location = 1)  flat in mat3 view_particle_matrix_fs;  // locations 1,2,3
layout(location = 4)  flat in vec3 particle_view_pos_fs;
layout(location = 5)  flat in vec2 particle_exponents_fs;
layout(location = 6)  noperspective in vec3 ray_origin_fs;
layout(location = 7)  in vec3 ray_dir_fs;
layout(location = 8)  flat in uint instanceIndex_fs;

#include "../../shading.glsl"

// The rasterized geometry is a back-face-culled bounding box that fully encloses
// the superquadric, so the analytic surface depth is always ≥ the front-face depth.
// depth_greater re-enables HiZ for Vulkan/Metal; the HLSL variant is patched at
// build time to plain SV_Depth (SPIRV-Cross omits noperspective_centroid on SV_Position
// when SV_DepthGreaterEqual is output, failing DXIL validation on Direct3D).
#ifndef OVITO_DISABLE_DEPTH_GREATER
layout(depth_greater) out float gl_FragDepth;
#else
out float gl_FragDepth;
#endif

const int PLANECOUNT = 9;
const vec4 _sqPlanes[PLANECOUNT] = vec4[](
    vec4(1.0, 1.0, 0.0, 0.0), vec4(1.0,-1.0, 0.0, 0.0),
    vec4(1.0, 0.0, 1.0, 0.0), vec4(1.0, 0.0,-1.0, 0.0),
    vec4(0.0, 1.0, 1.0, 0.0), vec4(0.0, 1.0,-1.0, 0.0),
    vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0),
    vec4(0.0, 0.0, 1.0, 0.0));

const float _sqEpsilon     = 1.0e-10;
const float _sqMinValue    = -1.01;
const float _sqMaxValue    =  1.01;
const float _sqBoundHuge   =  2.0e+10;
const float _sqDepthTol    =  1.0e-4;
const float _sqZeroTol     =  1.0e-10;
const int   _sqMaxIter     = 20;

bool _sqIntersectBox(in vec3 P, in vec3 D, out float dmin, out float dmax)
{
    float tmin = 0.0, tmax = 0.0;
    if(abs(D.x) > _sqEpsilon) {
        if(D.x > _sqEpsilon) { dmin = (_sqMinValue - P.x)/D.x; dmax = (_sqMaxValue - P.x)/D.x; }
        else                  { dmax = (_sqMinValue - P.x)/D.x; dmin = (_sqMaxValue - P.x)/D.x; }
        if(dmin > dmax) return false;
    } else {
        if(P.x < _sqMinValue || P.x > _sqMaxValue) return false;
        dmin = -_sqBoundHuge; dmax = _sqBoundHuge;
    }
    if(abs(D.y) > _sqEpsilon) {
        if(D.y > _sqEpsilon) { tmin = (_sqMinValue - P.y)/D.y; tmax = (_sqMaxValue - P.y)/D.y; }
        else                  { tmax = (_sqMinValue - P.y)/D.y; tmin = (_sqMaxValue - P.y)/D.y; }
        if(tmax < dmax) {
            if(tmin > dmin) { if(tmin > tmax) return false; dmin = tmin; }
            else            { if(dmin > tmax) return false; }
            dmax = tmax;
        } else {
            if(tmin > dmin) { if(tmin > dmax) return false; dmin = tmin; }
        }
    } else {
        if(P.y < _sqMinValue || P.y > _sqMaxValue) return false;
    }
    if(abs(D.z) > _sqEpsilon) {
        if(D.z > _sqEpsilon) { tmin = (_sqMinValue - P.z)/D.z; tmax = (_sqMaxValue - P.z)/D.z; }
        else                  { tmax = (_sqMinValue - P.z)/D.z; tmin = (_sqMaxValue - P.z)/D.z; }
        if(tmax < dmax) {
            if(tmin > dmin) { if(tmin > tmax) return false; dmin = tmin; }
            else            { if(dmin > tmax) return false; }
            dmax = tmax;
        } else {
            if(tmin > dmin) { if(tmin > dmax) return false; dmin = tmin; }
        }
    } else {
        if(P.z < _sqMinValue || P.z > _sqMaxValue) return false;
    }
    return true;
}

float _sqEvalG(in float x, in float y, in float e)
{
    float g = 0.0;
    if(x > y) {
        // abs() on the ratio silences FXC warning X3571 ("pow(f,e) will not work for
        // negative f"). Callers always pass abs() values so this does not change results.
        g = 1.0 + pow(abs(y/x), e);
        if(g != 1.0) g = pow(abs(g), 1.0/e);
        g *= x;
    } else if(y != 0.0) {
        g = 1.0 + pow(abs(x/y), e);
        if(g != 1.0) g = pow(abs(g), 1.0/e);
        g *= y;
    }
    return g;
}

float _sqEval(in vec3 P)
{
    return _sqEvalG(_sqEvalG(abs(P.x), abs(P.y), particle_exponents_fs.x), abs(P.z), particle_exponents_fs.y) - 1.0;
}

void _sqSolveHit1(in float v0, in vec3 tP0, in float v1, in vec3 tP1, out vec3 P)
{
    vec3 P0 = tP0, P1 = tP1;
    for(int i = 0; i < _sqMaxIter; i++) {
        if(abs(v0) < _sqZeroTol) { P = P0; return; }
        if(abs(v1) < _sqZeroTol) { P = P1; return; }
        float x = abs(v0) / abs(v1 - v0);
        vec3 P2 = P0 + x * (P1 - P0);
        float v2 = _sqEval(P2);
        vec3 P3 = P0 + 0.5 * (P1 - P0);
        float v3 = _sqEval(P3);
        if(v2 * v3 < 0.0) { v0 = v2; P0 = P2; v1 = v3; P1 = P3; }
        else if(abs(v2) < abs(v3)) {
            if(v0 * v2 < 0.0) { v1 = v2; P1 = P2; } else { v0 = v2; P0 = P2; }
        } else {
            if(v0 * v3 < 0.0) { v1 = v3; P1 = P3; } else { v0 = v3; P0 = P3; }
        }
    }
    P = (abs(v0) < abs(v1)) ? P0 : P1;
}

bool _sqCheckHit2(in vec3 Pbase, in vec3 D, in float t0, inout vec3 P0, in float v0, in float t1, out float t, out vec3 Q)
{
    const float eps = 1.0e-5;
    float dt0 = t0, dt1 = t0 + 1.0e-4 * (t1 - t0);
    float maxdelta = t1 - t0;
    for(int i = 0; (dt0 < t1) && (i < _sqMaxIter); i++) {
        vec3 P1 = Pbase + dt1 * D;
        float v1 = _sqEval(P1);
        if(v0 * v1 < 0.0) {
            _sqSolveHit1(v0, P0, v1, P1, Q);
            P0 = Q - Pbase;
            t = length(P0);
            return true;
        } else if(abs(v1) < eps) {
            Q = P1; t = dt1; return true;
        } else if(((v0 > 0.0) && (v1 > v0)) || ((v0 < 0.0) && (v1 < v0))) {
            break;
        } else if(v1 == v0) {
            break;
        } else {
            float deltat = v1 * (dt1 - dt0) / (v1 - v0);
            if(abs(deltat) > maxdelta) break;
            v0 = v1; dt0 = dt1; dt1 -= deltat; P0 = P1;
        }
    }
    return false;
}

SurfaceSample sampleSurface()
{
    vec3 ray_dir_norm = normalize(ray_dir_fs);

    // Shift ortho ray base to particle depth for numerical precision.
    vec3 ray_origin_shifted = ray_origin_fs;
    if(isPerspective == 0)
        ray_origin_shifted.z = particle_view_pos_fs.z;

    // Transform ray into particle (superellipsoid) space.
    vec3 P = view_particle_matrix_fs * (ray_origin_shifted - particle_view_pos_fs);
    vec3 D = view_particle_matrix_fs * ray_dir_norm;
    float len = length(D);
    D /= len;

    // tnear = _sqBoundHuge is the "no hit" sentinel used by the single discard below.
    // All early-exit conditions simply skip the computation that would set tnear, so
    // the final discard fires. Using a single discard avoids the spirv-cross HLSL
    // emitter bug ("argument pulled into unrelated predicate") triggered by multiple
    // OpKill instructions in the same function.
    float t1 = 0.0, t2 = 0.0;
    float tnear = _sqBoundHuge;

    if(_sqIntersectBox(P, D, t1, t2) && !(isPerspective != 0 && t2 < _sqDepthTol)) {
        if(isPerspective != 0) {
            if(t1 < _sqDepthTol) t1 = _sqDepthTol;
        } else {
            if(t1 < _sqDepthTol) {
                float shift = _sqDepthTol - t1;
                P -= D * shift;
                ray_origin_shifted -= ray_dir_norm * (shift / len);
                t2 += shift;
                t1 = _sqDepthTol;
            }
        }

        // Collect additional intersection depths with subdivision planes.
        int cnt = 2;
        float dists[PLANECOUNT + 2];
        dists[0] = t1; dists[1] = t2;
        float margin = _sqEpsilon * (t2 - t1);
        float mindist = t1 - margin, maxdist = t2 + margin;
        for(int i = 0; i < PLANECOUNT; i++) {
            float d = dot(D, _sqPlanes[i].xyz);
            if(abs(d) < _sqEpsilon) continue;
            float t = (_sqPlanes[i].w - dot(P, _sqPlanes[i].xyz)) / d;
            if(t >= mindist && t <= maxdist)
                dists[cnt++] = t;
        }

        // Bubble sort.
        bool done;
        do {
            done = true;
            for(int i = 1; i < cnt; i++) {
                if(dists[i] < dists[i-1]) {
                    float tmp = dists[i]; dists[i] = dists[i-1]; dists[i-1] = tmp;
                    done = false;
                }
            }
        } while(!done);

        // Walk intervals looking for a sign change (surface crossing).
        vec3 P0 = P + dists[0] * D;
        float v0 = _sqEval(P0);

        if(abs(v0) < _sqZeroTol) {
            tnear = dists[0] / len;
        } else {
            for(int i = 1; i < cnt; i++) {
                vec3 P1 = P + dists[i] * D;
                float v1 = _sqEval(P1);
                if(abs(v1) < _sqZeroTol) {
                    tnear = dists[i] / len;
                    break;
                } else if(v0 * v1 < 0.0) {
                    vec3 P2; _sqSolveHit1(v0, P0, v1, P1, P2);
                    tnear = length(P2 - P) / len;
                    break;
                } else {
                    float t; vec3 P2;
                    if(_sqCheckHit2(P, D, dists[i-1], P0, v0, dists[i], t, P2)) {
                        tnear = t / len;
                        break;
                    }
                }
                v0 = v1; P0 = P + dists[i] * D;
            }
        }
    }

    if(tnear == _sqBoundHuge || tnear < 0.0) discard;

    // Intersection in view space.
    vec3 view_hit = ray_origin_shifted + tnear * ray_dir_norm;

    float ze = view_hit.z;
    float zn = (projectionMatrix[2][2] * ze + projectionMatrix[3][2]) /
               (projectionMatrix[2][3] * ze + projectionMatrix[3][3]);

    // Surface point in particle space.
    vec3 Phit = P + D * (tnear * len);

    // Analytical gradient of the superellipsoid implicit function (= outward normal in particle space).
    float r = 0.0, z2n = 0.0;
    vec3 N = Phit;
    if(N.z != 0.0) {
        z2n = pow(abs(N.z), particle_exponents_fs.y);
        N.z = z2n / N.z;
    }
    if(abs(N.x) > abs(N.y)) {
        r = pow(abs(N.y / N.x), particle_exponents_fs.x);
        N.x = (1.0 - z2n) / N.x;
        N.y = (N.y != 0.0) ? (1.0 - z2n) * r / N.y : 0.0;
    } else if(N.y != 0.0) {
        r = pow(abs(N.x / N.y), particle_exponents_fs.x);
        N.x = (N.x != 0.0) ? (1.0 - z2n) * r / N.x : 0.0;
        N.y = (1.0 - z2n) / N.y;
    }
    if(N.z != 0.0) N.z *= (1.0 + r);

    // Transform normal from particle space to view space:
    // normal_view = transpose(view_particle_matrix) * N  (inverse-transpose of the forward transform).
    // In GLSL, (rowVec * M) treats rowVec as a row, giving transpose(M) * colVec.
    vec3 surface_normal = normalize(N * view_particle_matrix_fs);

    SurfaceSample s;
    s.viewPos    = view_hit;
    s.normal     = surface_normal;
    s.rayDir     = ray_dir_norm;
    s.color      = color_fs;
    s.pickingId  = instanceIndex_fs;
    gl_FragDepth = zn * 0.5 + 0.5;
    return s;
}
