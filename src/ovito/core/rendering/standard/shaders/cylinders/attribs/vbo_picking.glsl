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
