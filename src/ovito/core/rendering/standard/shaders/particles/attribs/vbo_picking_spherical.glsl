// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: VBO-based per-instance attributes for picking spherical particles.
// Only position and radius are bound; color fields are unused for picking.
// Bindings: 0=position(vec3), 1=radius(float).
// Provides: fetchParticle(uint idx) and getParticleIndex().

layout(location = 0) in vec3 position;
layout(location = 1) in float radius;

uint getParticleIndex() { return uint(gl_InstanceIndex); }

ParticleAttribs fetchParticle(uint idx)
{
    ParticleAttribs a;
    a.position      = position;
    a.radius        = radius;
    a.color         = vec4(0.0);
    a.orientation   = vec4(0.0, 0.0, 0.0, 1.0);
    a.principalAxes = vec3(0.0);
    a.roundness     = vec2(2.0);
    a.pickingId     = uint(gl_InstanceIndex);
    return a;
}
