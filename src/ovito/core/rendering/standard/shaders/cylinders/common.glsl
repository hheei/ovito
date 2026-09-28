// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Common data structures for cylinder/arrow vertex and fragment snippets.

// Unified per-cylinder attribute bundle populated by each attribs snippet.
struct CylinderAttribs {
    vec3  base;         // world-space base position
    vec3  head;         // world-space head position
    float width;        // cylinder diameter (world space)
    vec4  color1;       // rgba base color; selection-resolved, a = (1 - transparency1)
    vec4  color2;       // rgba head color; same as color1 if no dual-color
    uint  pickingId;    // instance index for picking output; 0 in the visual path
};

// Surface intersection result (same structure as used in particles).
// Must match the SurfaceSample struct expected by the reused particle fragout snippets.
struct SurfaceSample {
    vec3  viewPos;   // surface intersection point in view space
    float depth;     // NDC depth for gl_FragDepth (range 0..1)
    vec3  normal;    // outward surface normal in view space
    vec3  rayDir;    // normalized view-space ray direction
    vec4  color;     // unshaded surface color
    uint  pickingId; // instance index for picking output
};
