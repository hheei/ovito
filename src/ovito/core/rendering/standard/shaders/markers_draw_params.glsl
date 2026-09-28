// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

layout(std140, binding = 1) uniform MarkerDrawParams {
    mat4  modelViewProjectionMatrix; ///< proj * view * model.
    vec4  markerColor;               ///< Uniform RGBA color for all markers.
    float markerSize;                ///< Screen-space scale factor (= 4.0 / viewportHeight).
    uint  pickingBaseObjectId;
    float _markerPad0, _markerPad1;  ///< Padding to 16-byte alignment.
};
