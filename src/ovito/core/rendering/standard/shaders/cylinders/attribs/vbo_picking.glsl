// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: VBO per-instance attributes for picking pass (base, head, width + picking ID).
// Binding 0=base(vec3), 1=head(vec3), 2=width(float).
// Color attributes are not needed for picking.

layout(location = 0) in vec3 inBase;
layout(location = 1) in vec3 inHead;
layout(location = 2) in float inWidth;

uint getCylinderIndex() { return uint(gl_InstanceIndex); }

CylinderAttribs fetchCylinder(uint idx)
{
    CylinderAttribs a;
    a.base      = inBase;
    a.head      = inHead;
    a.width     = inWidth;
    a.color1    = vec4(0.0);
    a.color2    = vec4(0.0);
    a.pickingId = uint(gl_InstanceIndex);
    return a;
}
