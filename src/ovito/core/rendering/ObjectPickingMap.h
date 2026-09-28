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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectIdAllocator.h>
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

/**
 * \brief A mapping of frame buffer object IDs to picking groups.
 *
 * The two-component picking system uses (objectId, primitiveId) pairs rendered
 * to two R32UI color attachments:
 *   - objectId identifies the rendering command (must be > 0 for a valid hit).
 *   - primitiveId identifies the sub-object within that command.
 */
class OVITO_CORE_EXPORT ObjectPickingMap
{
public:

    /// Registers a unique object ID for a rendering command.
    uint32_t registerObjectId(ObjectIdAllocator::ObjectHandle baseObjectId, const FrameGraph::RenderingCommand& command, ConstDataBufferPtr subobjIndices = {}, bool derivePrimitiveIdFromObjectId = false);

	/// Releases all data held by the object.
	void reset() { _pickingRecords.clear(); }

	/// Resolves a (objectId, primitiveId, depth) triple from the picking buffer into a PickResult.
	/// Returns std::nullopt if the objectId is not found.
	std::optional<ViewportWindow::PickResult> resolvePickResult(
		uint32_t objectId, uint32_t primitiveId, float depth,
		const QPoint& pixelPos,
		const ViewProjectionParameters& projParams, QSize renderSize) const;

protected:

    /// Describes a pickable graphics primitive that has been encoded as a range of object IDs in the frame buffer.
	class PickingRecord
	{
	public:

		/// Constructor.
		explicit PickingRecord(const FrameGraph::RenderingCommand& command, ConstDataBufferPtr indices, bool derivePrimitiveIdFromObjectId) :
			_sceneNode(command.sceneNode()), _pickInfo(command.pickInfo()),
			_subobjectOffset(command.pickElementOffset()), _indices(std::move(indices)),
			_derivePrimitiveIdFromObjectId(derivePrimitiveIdFromObjectId) {}

		/// Returns the picked pipeline scene node.
		const OORef<const SceneNode>& sceneNode() const { return _sceneNode; }

		/// Returns an optional object that knows what high-level data was picked.
		const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

		/// Resolves the given 0-based primitive ID to an original sub-object index of a ObjectPickInfo.
		uint32_t translatePrimitiveIdToSubObject(uint32_t primitiveId) const {
			if(_indices) {
				OVITO_ASSERT(primitiveId >= 0 && primitiveId < _indices->size());
				primitiveId = BufferReadAccess<int32_t>(_indices).get(primitiveId);
			}
			return primitiveId + _subobjectOffset;
		}

		/// Indicates whether the primitive ID from the frame buffer should be ignored and instead the primitive ID should be derived
		/// from the offset of the object ID read from the frame buffer relative to the base object ID of this picking record.
		bool derivePrimitiveIdFromObjectId() const { return _derivePrimitiveIdFromObjectId; }

	private:

		/// If the renderer uses an indexed drawing command, this information allows mapping the packed primitive IDs in the frame buffer
		/// back to the original sub-object indices of the rendering primitive.
		ConstDataBufferPtr _indices;

		/// The pipeline scene node to which the picked rendering command belongs.
		/// Note: may be null in rare cases, e.g., when the AmbientOcclusionModifier renders particles using false colors.
		OORef<const SceneNode> _sceneNode;

		/// An optional object that knows what high-level data is being represented by this render command and which sub-elements it consists of.
		OORef<ObjectPickInfo> _pickInfo;

		/// If this rendering command is part of a composite object that requires multiple rendering commands,
		/// then this offset indicates where this command's primitive elements start in the composite range.
		uint32_t _subobjectOffset;

		/// Indicates that the primitive ID from the frame buffer should be ignored and instead the primitive ID should be derived
		/// from the offset of the object ID read from the frame buffer relative to the base object ID of this picking record.
		bool _derivePrimitiveIdFromObjectId;
	};

	/// The picking infos for the rendered graphics primitives, indexed by base object ID.
	/// std::less<> (transparent comparator) enables heterogeneous lookup by plain uint32_t
	/// via ObjectHandle's implicit operator uint32_t() conversion.
	std::map<ObjectIdAllocator::ObjectHandle, PickingRecord, std::less<>> _pickingRecords;
};

}   // End of namespace
