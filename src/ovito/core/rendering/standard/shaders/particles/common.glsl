// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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
