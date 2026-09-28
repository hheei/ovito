// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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
