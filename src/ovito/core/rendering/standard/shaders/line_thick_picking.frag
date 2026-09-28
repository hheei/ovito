// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "lines_draw_params.glsl"

// Instance index passed from vertex shader.
layout(location = 0) flat in uint instanceIndex_fs;

// Two R32UI output attachments for the picking render target.
layout(location = 0) out uint out_objectId;
layout(location = 1) out uint out_primitiveId;

void main()
{
    out_objectId = pickingBaseObjectId;
    // Each instance is one line segment.
    out_primitiveId = instanceIndex_fs;
}
