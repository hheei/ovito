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

// Attribs snippet: VBO-based per-instance attributes for visual superquadric particles.
// Bindings: 0=position(vec3), 1=radius(float), 2=color(vec3), 3=transparency(float),
//           4=selection(UNormByte), 5=asphericalShapes(vec3), 6=orientation(vec4), 7=roundness(vec2).
// Provides: fetchParticle(uint idx) and getParticleIndex().

layout(location = 0) in vec3 position;
layout(location = 1) in float radius;
layout(location = 2) in vec3 color;
layout(location = 3) in float transparency;
layout(location = 4) in float selection;
layout(location = 5) in vec3 asphericalShapes;
layout(location = 6) in vec4 orientation;
layout(location = 7) in vec2 roundness;

uint getParticleIndex() { return uint(gl_InstanceIndex); }

ParticleAttribs fetchParticle(uint idx)
{
    ParticleAttribs a;
    a.position      = position;
    a.radius        = radius;
    float alpha     = clamp(1.0 - transparency, 0.0, 1.0);
    a.color         = (selection != 0.0) ? selectionColor : vec4(color, alpha);
    a.orientation   = orientation;
    a.principalAxes = asphericalShapes;
    a.roundness     = roundness;
    a.pickingId     = 0u;
    return a;
}
