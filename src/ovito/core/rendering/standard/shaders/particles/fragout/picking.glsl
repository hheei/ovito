// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// FragOut snippet: particle picking pass.
// Writes (objectId, primitiveId) to two R32UI output attachments.
// objectId comes from the DrawParams UBO; primitiveId is the particle index from the surface sample.

layout(location = 0) out uint out_objectId;
layout(location = 1) out uint out_primitiveId;

void writeOut(SurfaceSample s)
{
    out_objectId  = pickingBaseObjectId;
    out_primitiveId = s.pickingId;
}
