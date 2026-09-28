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

layout(std140, binding = 1) uniform MarkerDrawParams {
    mat4  modelViewProjectionMatrix; ///< proj * view * model.
    vec4  markerColor;               ///< Uniform RGBA color for all markers.
    float markerSize;                ///< Screen-space scale factor (= 4.0 / viewportHeight).
    uint  pickingBaseObjectId;
    float _markerPad0, _markerPad1;  ///< Padding to 16-byte alignment.
};
