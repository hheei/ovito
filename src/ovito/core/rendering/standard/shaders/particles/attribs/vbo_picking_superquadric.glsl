// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: VBO-based per-instance attributes for picking superquadric particles.
// Bindings: 0=position(vec3), 1=radius(float), 2=asphericalShapes(vec3),
//           3=orientation(vec4), 4=roundness(vec2).
// Color attributes omitted; pickingId set from instance index.
// Provides: fetchParticle(uint idx) and getParticleIndex().

layout(location = 0) in vec3 position;
layout(location = 1) in float radius;
layout(location = 2) in vec3 asphericalShapes;
layout(location = 3) in vec4 orientation;
layout(location = 4) in vec2 roundness;

uint getParticleIndex() { return uint(gl_InstanceIndex); }

ParticleAttribs fetchParticle(uint idx)
{
    ParticleAttribs a;
    a.position      = position;
    a.radius        = radius;
    a.color         = vec4(0.0);
    a.orientation   = orientation;
    a.principalAxes = asphericalShapes;
    a.roundness     = roundness;
    a.pickingId     = uint(gl_InstanceIndex);
    return a;
}
