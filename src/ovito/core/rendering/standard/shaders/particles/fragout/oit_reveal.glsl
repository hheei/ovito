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

// FragOut snippet: Weighted Blended OIT reveal pass.
// Writes transparency (1 - alpha) to all channels of revealOut.
// Render with multiplicative blending (src=Zero, dst=SrcColor).
// The blend equation accumulates: reveal = product(1 - alpha_i) across overlapping fragments.

layout(location = 0) out vec4 revealOut;

void writeOut(SurfaceSample s)
{
    float transparency = 1.0 - s.color.a;
    revealOut = vec4(transparency, transparency, transparency, transparency);
}
