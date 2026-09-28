// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

/**
 * \brief The result of an offscreen picking pass: everything needed to determine the object at any position.
 *
 * A picking pass renders the object IDs and the primitive (sub-object) IDs of a frame into two offscreen
 * textures plus a depth texture, and records an ObjectPickingMap while doing so. This class holds the CPU
 * copies of those three buffers together with that map and the projection the pass was rendered with.
 *
 * The important property is that a picking buffer is self-contained: it belongs to the caller, requires no
 * access to the GPU, the render service or the render thread, and can therefore be queried from any thread.
 * That decouples object picking from the frame-based rendering of a viewport, which is what allows the Qt Quick
 * frontend to serve pick requests from a buffer that was rendered asynchronously in the background instead of
 * blocking its event loop until the next frame has been rendered.
 */
class OVITO_CORE_EXPORT ObjectPickingBuffer
{
public:

    /// Constructs an invalid buffer holding no picking data.
    ObjectPickingBuffer() = default;

    /// Constructor.
    ObjectPickingBuffer(ObjectPickingMap pickingMap, QByteArray objectIdData, QByteArray primitiveIdData, QByteArray depthData,
        const QSize& bufferSize, const ViewProjectionParameters& projParams) :
        _pickingMap(std::move(pickingMap)),
        _objectIdData(std::move(objectIdData)),
        _primitiveIdData(std::move(primitiveIdData)),
        _depthData(std::move(depthData)),
        _bufferSize(bufferSize),
        _projParams(projParams) {}

    /// Indicates whether the buffer holds usable picking data. An invalid buffer is returned when the
    /// picking pass could not be rendered.
    bool isValid() const { return _bufferSize.width() > 0 && _bufferSize.height() > 0 && !_objectIdData.isEmpty(); }

    /// Returns the pixel size of the picking buffer.
    const QSize& bufferSize() const { return _bufferSize; }

    /// Determines the object located at the given position of the buffer.
    /// \param pos        Position in picking buffer (device pixel) coordinates.
    /// \param pickRadius Radius in pixels to search around \a pos for a rendered object.
    /// \return The picked object or an empty optional if no object was rendered at that position.
    std::optional<ViewportWindow::PickResult> pick(const QPointF& pos, int pickRadius) const
    {
        if(!isValid())
            return std::nullopt;
        return _pickingMap.lookupPickResult(_objectIdData, _primitiveIdData, _depthData,
            _bufferSize, pos, pickRadius, _projParams);
    }

private:

    /// Maps the object IDs rendered into the picking buffer back to scene nodes and sub-objects.
    ObjectPickingMap _pickingMap;

    /// CPU copies of the object ID, primitive ID and depth textures of the picking pass.
    QByteArray _objectIdData;
    QByteArray _primitiveIdData;
    QByteArray _depthData;

    /// Pixel size of the picking textures the data was read back from.
    QSize _bufferSize;

    /// Projection parameters the picking pass was rendered with.
    ViewProjectionParameters _projParams;
};

}   // End of namespace
