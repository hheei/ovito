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

// Common data structures shared across all particle vertex and fragment snippets.

// Unified per-particle attribute bundle populated by each attribs snippet.
// Fields unused by a given shape are set to their neutral defaults by the attribs snippet;
// the SPIR-V compiler eliminates any field the downstream geometry/surface snippets never read.
struct ParticleAttribs {
    vec3  position;      // world-space center
    float radius;        // scalar size (world-space units)
    vec4  color;         // rgba; selection-resolved, a = (1 - transparency)
    vec4  orientation;   // unit quaternion (x,y,z,w); (0,0,0,1) if unused
    vec3  principalAxes; // per-axis scale factors for oriented shapes; (1,1,1) if unused
    vec2  roundness;     // superquadric shape exponents; (2.0, 2.0) for sphere
    uint  pickingId;     // particle index for picking output; 0 in the visual path
};

// Surface intersection result produced by each surface snippet's sampleSurface().
struct SurfaceSample {
    vec3  viewPos;   // surface intersection point in view space
    vec3  normal;    // outward surface normal in view space (need not be front-facing)
    vec3  rayDir;    // normalized view-space ray direction
    vec4  color;     // unshaded particle color carried from the vertex varyings
    uint  pickingId; // particle index for the picking fragout
};
