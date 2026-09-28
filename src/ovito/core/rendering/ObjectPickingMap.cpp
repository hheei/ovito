// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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

/******************************************************************************
* Searches the picking buffer for the closest pixel belonging to a rendered
* object and resolves it into a pick result.
******************************************************************************/
std::optional<ViewportWindow::PickResult> ObjectPickingMap::lookupPickResult(
    const QByteArray& objectIdData, const QByteArray& primitiveIdData, const QByteArray& depthData,
    const QSize& bufferSize, const QPointF& pos, int pickRadius,
    const ViewProjectionParameters& projParams) const
{
    const int w = bufferSize.width();
    const int h = bufferSize.height();
    if(w <= 0 || h <= 0)
        return std::nullopt;
    // Make sure the readback buffers really contain one 32-bit value per pixel.
    if(objectIdData.size() < w * h * int(sizeof(uint32_t))
        || primitiveIdData.size() < w * h * int(sizeof(uint32_t))
        || depthData.size() < w * h * int(sizeof(float)))
        return std::nullopt;

    const int cx = qBound(0, static_cast<int>(pos.x()), w - 1);
    const int cy = qBound(0, static_cast<int>(pos.y()), h - 1);
    const uint32_t* objectIds  = reinterpret_cast<const uint32_t*>(objectIdData.constData());
    const uint32_t* primitiveIds = reinterpret_cast<const uint32_t*>(primitiveIdData.constData());

    // A pixel is considered "hit" when its objectId is non-zero.
    auto isHit = [&](int x, int y) -> bool {
        return objectIds[y * w + x] != 0;
    };

    // Check the center pixel first.
    int foundX = cx, foundY = cy;
    bool found = isHit(cx, cy);
    if(!found) {
        // Search outward in concentric rings.
        int bestDistSq = std::numeric_limits<int>::max();
        for(int ring = 1; ring <= pickRadius; ring++) {
            for(int d = -ring; d <= ring; d++) {
                // Top and bottom edges.
                for(int ey : { cy - ring, cy + ring }) {
                    int ex = cx + d;
                    if(ex >= 0 && ex < w && ey >= 0 && ey < h && isHit(ex, ey)) {
                        int distSq = d * d + ring * ring;
                        if(distSq < bestDistSq) {
                            bestDistSq = distSq;
                            foundX = ex;
                            foundY = ey;
                            found = true;
                        }
                    }
                }
                // Left and right edges (excluding corners, already handled above).
                if(d != -ring && d != ring) {
                    for(int ex : { cx - ring, cx + ring }) {
                        int ey = cy + d;
                        if(ex >= 0 && ex < w && ey >= 0 && ey < h && isHit(ex, ey)) {
                            int distSq = ring * ring + d * d;
                            if(distSq < bestDistSq) {
                                bestDistSq = distSq;
                                foundX = ex;
                                foundY = ey;
                                found = true;
                            }
                        }
                    }
                }
            }
            if(found) break;
        }
    }

    if(!found)
        return std::nullopt;

    const uint32_t objectId    = objectIds[foundY * w + foundX];
    const uint32_t primitiveId = primitiveIds[foundY * w + foundX];
    const float    depth       = reinterpret_cast<const float*>(depthData.constData())[foundY * w + foundX];

    return resolvePickResult(objectId, primitiveId, depth, QPoint(foundX, foundY), projParams, bufferSize);
}

}   // End of namespace
