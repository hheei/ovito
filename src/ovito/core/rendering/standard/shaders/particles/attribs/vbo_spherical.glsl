// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: VBO-based per-instance attributes for visual spherical particles.
// Bindings: 0=position(vec3), 1=radius(float), 2=color(vec3), 3=transparency(float), 4=selection(UNormByte).
// Provides: fetchParticle(uint idx) and getParticleIndex().

layout(location = 0) in vec3 position;
layout(location = 1) in float radius;
layout(location = 2) in vec3 color;
layout(location = 3) in float transparency;
layout(location = 4) in float selection;

uint getParticleIndex() { return uint(gl_InstanceIndex); }

ParticleAttribs fetchParticle(uint idx)
{
    ParticleAttribs a;
    a.position      = position;
    a.radius        = radius;
    float alpha     = clamp(1.0 - transparency, 0.0, 1.0);
    a.color         = (selection != 0.0) ? selectionColor : vec4(color, alpha);
    a.orientation   = vec4(0.0, 0.0, 0.0, 1.0);
    a.principalAxes = vec3(0.0);
    a.roundness     = vec2(2.0);
    a.pickingId     = 0u;
    return a;
}
