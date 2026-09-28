////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include "ObjectPickingMap.h"

namespace Ovito {

/******************************************************************************
* Registers a unique object ID for a rendering command.
******************************************************************************/
uint32_t ObjectPickingMap::registerObjectId(ObjectIdAllocator::ObjectHandle baseObjectId, const FrameGraph::RenderingCommand& command, ConstDataBufferPtr subobjIndices, bool derivePrimitiveIdFromObjectId)
{
    OVITO_ASSERT(!command.skipInPickingPass());
    OVITO_ASSERT(!subobjIndices || subobjIndices->size() != 0);
    OVITO_ASSERT(baseObjectId);
    OVITO_ASSERT(_pickingRecords.find(baseObjectId) == _pickingRecords.end());

    uint32_t id = baseObjectId;
    _pickingRecords.emplace(std::move(baseObjectId), PickingRecord(command, std::move(subobjIndices), derivePrimitiveIdFromObjectId));
    return id;
}

/******************************************************************************
* Resolves a (objectId, primitiveId, depth) triple from the picking buffer
* into a ViewportWindow::PickResult. Returns std::nullopt if not found.
******************************************************************************/
std::optional<ViewportWindow::PickResult> ObjectPickingMap::resolvePickResult(
    uint32_t objectId, uint32_t primitiveId, float depth, const QPoint& pixelPos,
    const ViewProjectionParameters& projParams, QSize renderSize) const
{
    // The highest bit of the instance/object ID is reserved by the ANARI/OSPRay backends to
    // flag ExcludeFromOutline geometry (used by the outline effect). Strip it before resolving,
    // so the picking records — keyed on the unflagged allocator IDs — are looked up correctly.
    objectId &= 0x7FFFFFFFu;

    if(objectId == 0 || _pickingRecords.empty())
        return std::nullopt;

    // Find the records that succeeds the one containing the given object ID.
    uint32_t baseObjectId;
    const PickingRecord* record;
    auto iter = _pickingRecords.upper_bound(objectId);
    if(iter != _pickingRecords.end()) {
        OVITO_ASSERT(iter->first > objectId);
        // If objectId precedes all registered base IDs, it's invalid (e.g. stale picking buffer).
        if(iter == _pickingRecords.begin())
            return std::nullopt;
        OVITO_ASSERT(objectId >= std::prev(iter)->first);
        baseObjectId = std::prev(iter)->first;
        record = &std::prev(iter)->second;
    }
    else {
        OVITO_ASSERT(objectId >= _pickingRecords.crbegin()->first);
        baseObjectId = _pickingRecords.crbegin()->first;
        record = &_pickingRecords.crbegin()->second;
    }

    if(record->derivePrimitiveIdFromObjectId()) {
        // Derive the primitive ID from the offset of the object ID relative to the base object ID of this picking record.
        primitiveId = objectId - baseObjectId;
    }

    // Resolve the sub-object ID from the primitiveId.
    uint32_t subobjectId = record->translatePrimitiveIdToSubObject(primitiveId);

    // Reconstruct the 3D world-space hit location from the pixel position and depth value.
    // depth is in [0,1] from the D32F texture; convert to OpenGL NDC z in [-1,1].
    // pixelPos is in screen coordinates (y=0 at top); convert to NDC y in [-1,1] (y=1 at top).
    Point3 hitLocation = Point3::Origin();
    if(depth != 0.0f && !renderSize.isEmpty()) {
        Point3 ndc(
            (FloatType)pixelPos.x() / renderSize.width() * 2 - 1,
            1 - (FloatType)pixelPos.y() / renderSize.height() * 2,
            (FloatType)depth * 2 - 1);
        hitLocation = projParams.inverseViewMatrix * (projParams.inverseProjectionMatrix * ndc);
    }

    return ViewportWindow::PickResult(
        const_cast<SceneNode*>(record->sceneNode().get()),
        const_cast<ObjectPickInfo*>(record->pickInfo().get()),
        hitLocation,
        subobjectId);
}

}   // End of namespace
