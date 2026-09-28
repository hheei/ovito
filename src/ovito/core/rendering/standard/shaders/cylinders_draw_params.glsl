// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Per-draw-call parameters for cylinder/arrow rendering.
// std140 layout must match CylinderDrawParamsData in CylinderPrimitiveRenderer.cpp.

layout(std140, binding = 1) uniform CylinderDrawParams {
    mat4  modelViewMatrix;          // offset   0, 64 bytes
    vec4  selectionColor;           // offset  64, 16 bytes
    float uniformWidth;             // offset  80,  4 bytes  (cylinder diameter when no per-cyl widths)
    uint  pickingBaseObjectId;      // offset  84,  4 bytes
    float colorRangeMin;            // offset  88,  4 bytes  (pseudo-color range; min==max disables)
    float colorRangeMax;            // offset  92,  4 bytes
    vec4  viewDirEyePos;            // offset  96, 16 bytes  (xyz = view dir or eye pos in object space, w = isPerspective)
    int   singleCylinderCap;        // offset 112,  4 bytes  (1 = render only base cap)
    float _cylPad0;                 // offset 116
    float _cylPad1;                 // offset 120
    float _cylPad2;                 // offset 124
};  // 128 bytes total
