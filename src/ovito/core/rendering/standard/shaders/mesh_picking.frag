// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Mesh picking fragment shader.
// Outputs object ID and primitive ID to the two R32UI picking render target attachments.
// Shared by non-instanced and instanced mesh picking passes.

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "mesh_draw_params.glsl"

layout(location = 0) flat in uint primitiveId_fs;

layout(location = 0) out uint out_objectId;
layout(location = 1) out uint out_primitiveId;

void main()
{
    out_objectId   = pickingBaseObjectId;
    out_primitiveId = primitiveId_fs;
}
