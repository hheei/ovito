// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: VBO per-instance attributes for visual rendering with RGB colors.
// Binding 0=base(vec3), 1=head(vec3), 2=width(float),
//         3=color1(vec3), 4=color2(vec3), 5=transp1(float), 6=transp2(float), 7=selection(UNormByte).

layout(location = 0) in vec3 inBase;
layout(location = 1) in vec3 inHead;
layout(location = 2) in float inWidth;
layout(location = 3) in vec3 inColor1;
layout(location = 4) in vec3 inColor2;
layout(location = 5) in float inTransp1;
layout(location = 6) in float inTransp2;
layout(location = 7) in float inSelection;

uint getCylinderIndex() { return uint(gl_InstanceIndex); }

CylinderAttribs fetchCylinder(uint idx)
{
    CylinderAttribs a;
    a.base      = inBase;
    a.head      = inHead;
    a.width     = inWidth;
    a.pickingId = 0u;

    float alpha1 = clamp(1.0 - inTransp1, 0.0, 1.0);
    float alpha2 = clamp(1.0 - inTransp2, 0.0, 1.0);

    if(inSelection != 0.0) {
        a.color1 = selectionColor;
        a.color2 = selectionColor;
    } else {
        a.color1 = vec4(inColor1, alpha1);
        a.color2 = vec4(inColor2, alpha2);
    }
    return a;
}
