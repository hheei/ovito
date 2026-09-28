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

// Attribs snippet: VBO-based per-instance attributes for visual cylinder/arrow rendering.
// Bindings: 0=basePos(vec3), 1=headPos(vec3), 2=width(float),
//           3=color1(vec3), 4=color2(vec3), 5=transp1(float), 6=transp2(float), 7=selection(UNormByte).
// When PSEUDO_COLOR is defined: bindings 3 and 4 are float (scalar pseudo-color values).

layout(location = 0) in vec3 inBase;
layout(location = 1) in vec3 inHead;
layout(location = 2) in float inWidth;
#ifdef PSEUDO_COLOR
layout(location = 3) in float inColor1;
layout(location = 4) in float inColor2;
#else
layout(location = 3) in vec3 inColor1;
layout(location = 4) in vec3 inColor2;
#endif
layout(location = 5) in float inTransp1;
layout(location = 6) in float inTransp2;
layout(location = 7) in float inSelection;

uint getCylinderIndex() { return uint(gl_InstanceIndex); }

CylinderAttribs fetchCylinder(uint idx)
{
    CylinderAttribs a;
    a.base     = inBase;
    a.head     = inHead;
    a.width    = inWidth;
    a.pickingId = 0u;

    float alpha1 = clamp(1.0 - inTransp1, 0.0, 1.0);
    float alpha2 = clamp(1.0 - inTransp2, 0.0, 1.0);

    if(inSelection != 0.0) {
        a.color1 = selectionColor;
        a.color2 = selectionColor;
    } else {
#ifdef PSEUDO_COLOR
        // Map scalar pseudo-color values through the color map texture.
        vec3 col1 = (colorRangeMin != colorRangeMax)
            ? texture(colorMap, vec2((inColor1 - colorRangeMin) / (colorRangeMax - colorRangeMin), 0.5)).rgb
            : vec3(1.0);
        vec3 col2 = (colorRangeMin != colorRangeMax)
            ? texture(colorMap, vec2((inColor2 - colorRangeMin) / (colorRangeMax - colorRangeMin), 0.5)).rgb
            : vec3(1.0);
        a.color1 = vec4(col1, alpha1);
        a.color2 = vec4(col2, alpha2);
#else
        a.color1 = vec4(inColor1, alpha1);
        a.color2 = vec4(inColor2, alpha2);
#endif
    }
    return a;
}
