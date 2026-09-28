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

// Per-draw-call parameters for mesh rendering.
// std140 layout must match MeshDrawParamsData in MeshPrimitiveRenderer.cpp.

layout(std140, binding = 1) uniform MeshDrawParams {
    mat4  modelViewMatrix;      // offset   0, 64 bytes
    vec4  selectionColor;       // offset  64, 16 bytes
    vec4  wireframeColor;       // offset  80, 16 bytes
    float opacity;              // offset  96,  4 bytes
    uint  pickingBaseObjectId;  // offset 100,  4 bytes
    float colorRangeMin;        // offset 104,  4 bytes
    float colorRangeMax;        // offset 108,  4 bytes
    float lineThickness;        // offset 112,  4 bytes  (= wireframeWidth / viewportHeight)
    float _meshPad0;            // offset 116
    float _meshPad1;            // offset 120
    float _meshPad2;            // offset 124
};  // 128 bytes total
