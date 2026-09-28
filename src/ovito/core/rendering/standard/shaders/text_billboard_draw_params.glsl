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

layout(std140, binding = 1) uniform TextBillboardDrawParams {
    mat4 modelViewMatrix;      ///< view * model (the projection comes from SceneParams).
    vec2 alignmentFactor;      ///< Fraction of the quad size between anchor and quad top-left corner.
    vec2 shiftDirection;       ///< Unit vector (window coords, y down) of the radius shift; may be zero.
    vec2 pixelOffset;          ///< Constant label offset in device pixels (window coords).
    float depthOffset;         ///< Extra camera-facing shift in world units, added to the label radius.
    float _tbPad0;             ///< Padding to 16-byte alignment.
};
