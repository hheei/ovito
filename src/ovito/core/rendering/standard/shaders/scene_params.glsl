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

/// Must match the layout of the SceneParamsData struct in StandardRendererImplementation::prepareResourceUpdates().
/// This is a std140 uniform block, so padding must be observed.
layout(std140, binding = 0) uniform SceneParams {
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 inverseProjectionMatrix;
    mat4 clipSpaceCorrMatrix;
    mat4 clipProjectionMatrix; // = clipSpaceCorrMatrix * projectionMatrix (precomputed on CPU)
    float viewportWidth;
    float viewportHeight;
    int isYUpInNDC;
    int isYUpInFramebuffer;
    int isPerspective;
    float _pad0; float _pad1; float _pad2;
};
