// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

layout(std140, binding = 1) uniform LineDrawParams {
    mat4 modelViewProjectionMatrix; ///< proj * view * model, or identity for pre-projected NDC coordinates.
    float lineThickness;            ///< Half line width in normalized viewport height units (= lineWidth / viewportHeight).
    uint pickingBaseObjectId;
    float _linePad0, _linePad1;     ///< Padding to 16 byte alignment.
};
