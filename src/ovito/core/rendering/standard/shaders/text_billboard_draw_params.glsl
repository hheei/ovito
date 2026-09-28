// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

layout(std140, binding = 1) uniform TextBillboardDrawParams {
    mat4 modelViewMatrix;      ///< view * model (the projection comes from SceneParams).
    vec2 alignmentFactor;      ///< Fraction of the quad size between anchor and quad top-left corner.
    vec2 shiftDirection;       ///< Unit vector (window coords, y down) of the radius shift; may be zero.
    vec2 pixelOffset;          ///< Constant label offset in device pixels (window coords).
    float depthOffset;         ///< Extra camera-facing shift in world units, added to the label radius.
    float _tbPad0;             ///< Padding to 16-byte alignment.
};
