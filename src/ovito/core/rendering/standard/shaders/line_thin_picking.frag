// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "lines_draw_params.glsl"

// Vertex index passed from vertex shader.
layout(location = 0) flat in uint vertexIndex_fs;

// Two R32UI output attachments for the picking render target.
layout(location = 0) out uint out_objectId;
layout(location = 1) out uint out_primitiveId;

void main()
{
    out_objectId = pickingBaseObjectId;
    // Two vertices per line segment; divide vertex index by 2 to get segment index.
    out_primitiveId = vertexIndex_fs / 2u;
}
