// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

layout(std140, binding = 1) uniform DrawParams {
    mat4 modelViewMatrix;
    vec4 selectionColor;
    float uniformRadius;
    uint pickingBaseObjectId;
    float uniformModelScale; // = length(modelViewMatrix[0].xyz), precomputed on CPU
    float _drawPad1;
};
