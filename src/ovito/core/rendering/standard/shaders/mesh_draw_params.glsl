// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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
