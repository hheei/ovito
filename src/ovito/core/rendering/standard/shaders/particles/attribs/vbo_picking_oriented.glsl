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

// Attribs snippet: VBO-based per-instance attributes for picking oriented particles (box, ellipsoid).
// Bindings: 0=position(vec3), 1=radius(float), 2=asphericalShapes(vec3), 3=orientation(vec4).
// Color attributes omitted; pickingId set from instance index.
// Provides: fetchParticle(uint idx) and getParticleIndex().

layout(location = 0) in vec3 position;
layout(location = 1) in float radius;
layout(location = 2) in vec3 asphericalShapes;
layout(location = 3) in vec4 orientation;

uint getParticleIndex() { return uint(gl_InstanceIndex); }

ParticleAttribs fetchParticle(uint idx)
{
    ParticleAttribs a;
    a.position      = position;
    a.radius        = radius;
    a.color         = vec4(0.0);
    a.orientation   = orientation;
    a.principalAxes = asphericalShapes;
    a.roundness     = vec2(2.0);
    a.pickingId     = uint(gl_InstanceIndex);
    return a;
}
