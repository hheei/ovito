// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: VBO per-instance attributes for visual rendering with pseudo-color scalars.
// Binding 0=base(vec3), 1=head(vec3), 2=width(float),
//         3=color1(float scalar), 4=color2(float scalar), 5=transp1(float), 6=transp2(float), 7=selection(UNormByte).
// Scalars are passed as color.r; color.g = -1.0 signals the fragment shader to apply the colormap.

layout(location = 0) in vec3 inBase;
layout(location = 1) in vec3 inHead;
layout(location = 2) in float inWidth;
layout(location = 3) in float inPseudo1;
layout(location = 4) in float inPseudo2;
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
        float range = colorRangeMax - colorRangeMin;
        float t1 = (range != 0.0) ? clamp((inPseudo1 - colorRangeMin) / range, 0.0, 1.0) : 0.5;
        float t2 = (range != 0.0) ? clamp((inPseudo2 - colorRangeMin) / range, 0.0, 1.0) : 0.5;
        // Store normalized scalar in .r; .g = -1.0 signals the fragment shader to apply the colormap
        // after interpolation, ensuring correct per-pixel color mapping rather than interpolating
        // between pre-mapped endpoint colors.
        a.color1 = vec4(t1, -1.0, 0.0, alpha1);
        a.color2 = vec4(t2, -1.0, 0.0, alpha2);
    }
    return a;
}
