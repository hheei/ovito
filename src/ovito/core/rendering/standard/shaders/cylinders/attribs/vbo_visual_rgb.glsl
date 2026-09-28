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
