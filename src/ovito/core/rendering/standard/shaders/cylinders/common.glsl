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
