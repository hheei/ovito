// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/base/viewport/BaseViewportWindow.h>
#include <ovito/core/rendering/ObjectPickingBuffer.h>
#include <ovito/core/rendering/RenderThread.h>

namespace Ovito {

/**
 * \brief Connects a viewport of the current dataset to a QuickViewportItem in the QML scene.
 *
 * This class plays the role that WidgetViewportWindow plays for the classic QtWidgets frontend: it derives from
 * BaseViewportWindow to reuse OVITO's viewport input handling, scene preparation and frame graph generation, but it
 * does not own a native window or a widget. Instead, the QML item that is placed in the Qt Quick scene
 * (see QuickViewportItem) drives the rendering and forward the input events to this window.
 *
 * Note that QuickViewportItem and this class are separate objects on purpose: QQuickRhiItem and ViewportWindow
 * both derive from QObject, so a single class cannot inherit both (see docs/design/UI_DESIGN.md section 4.2).
 */
class OVITO_GUIQML_EXPORT QuickViewportWindow : public BaseViewportWindow
{
    Q_OBJECT
    OVITO_CLASS(QuickViewportWindow)

public:

    /// Constructor.
    QuickViewportWindow();

    /// Associates this window with a viewport and the QML item it renders into.
    void initializeWindow(Viewport* viewport, UserInterface& userInterface, QuickViewportItem* item);

    /// Returns the QML item this viewport window is rendered into.
    QuickViewportItem* item() const { return _item; }

    /// Handles show events.
    void handleShowEvent();

    /// Handles hide events.
    void handleHideEvent();

    /// Handles double click events.
    void handleMouseDoubleClick(QMouseEvent* event) { mouseDoubleClickEvent(event); }

    /// Handles mouse press events.
    void handleMousePress(QMouseEvent* event) { mousePressEvent(event); }

    /// Handles mouse release events.
    void handleMouseRelease(QMouseEvent* event) { mouseReleaseEvent(event); }

    /// Handles mouse move events.
    void handleMouseMove(QMouseEvent* event) { mouseMoveEvent(event); }

    /// Handles mouse wheel events.
    void handleWheel(QWheelEvent* event) { wheelEvent(event); }

    /// Handles key-press events.
    void handleKeyPress(QKeyEvent* event) { keyPressEvent(event); }

    /// Is called when the item looses the input focus.
    void handleFocusOut(QFocusEvent* event) { focusOutEvent(event); }

    /// Is called when the mouse cursor leaves the item.
    void handleLeave(QEvent* event) { leaveEvent(event); }

    /// Is called when the item is resized.
    void handleResize();

    /// Indicates whether the item is currently shown and renderable.
    bool isVisible() const override;

    /// Returns the current size of the viewport window (in device pixels).
    QSize viewportWindowDeviceSize() const override;

    /// Returns the current size of the viewport window (in device-independent pixels).
    QSize viewportWindowDeviceIndependentSize() const override;

    /// Returns the device pixel ratio of the viewport window's canvas.
    qreal devicePixelRatio() const override;

    /// Determines the object located under the given mouse cursor position.
    /// \param pos  Position of the mouse cursor in device-independent (logical) viewport coordinates.
    ///
    /// The picking pass is rendered offscreen by the render thread, independently of the interactive frame, so
    /// that picking neither blocks the GUI thread nor the Qt Quick scene graph. The result is taken from the
    /// picking buffer that was rendered most recently; if that buffer no longer matches the viewport contents, a
    /// new pass is started in the background and later calls are served from its result.
    std::optional<PickResult> pick(const QPointF& pos) override;

    /// Returns the icon image used for displaying non-fatal rendering warnings.
    QImage warningIcon() const override;

public Q_SLOTS:

    /// Releases the renderer resources held by this viewport window.
    void releaseResources() override;

protected:

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted.
    void aboutToBeDeleted() override;

    /// Hands the frame graph of the next frame over to the QML item for rendering.
    void renderFrameGraph(OORef<FrameGraph> frameGraph) override;

    /// A coroutine that renders a picking pass for the current viewport contents on the render thread.
    /// Returns an invalid buffer if the picking pass could not be rendered, e.g. because the viewport
    /// has been hidden in the meantime.
    Future<ObjectPickingBuffer> renderPickingBuffer();

private Q_SLOTS:

    /// Stores the result of the asynchronous picking pass.
    void pickingBufferReady();

    /// Reports a picking pass that terminated with an error instead of producing a buffer.
    void pickingBufferFailed(const Exception& exception);

private:

    /// Starts rendering a new picking buffer if no other picking pass is currently in flight.
    void refreshPickingBuffer();

    /// Radius (in device pixels) around the pick position in which a rendered object is searched for.
    static constexpr int pickRadius = 4;

    /// The QML item that renders this viewport. Not owning.
    QPointer<QuickViewportItem> _item;

    /// The picking buffer produced by the last completed offscreen picking pass.
    ObjectPickingBuffer _pickingBuffer;

    /// Watches the asynchronous picking pass rendering.
    FutureWatcher<Future<ObjectPickingBuffer>> _pickingBufferWatcher;

    /// The offscreen render target the picking passes are rendered into. Created on demand.
    std::optional<RenderTarget> _pickingTarget;

    /// The size the current picking render target was created for.
    QSize _pickingTargetSize;

    /// Indicates that the cached picking buffer no longer matches the viewport contents.
    bool _pickingBufferStale = true;

    /// Indicates that the failure of a picking pass has already been reported to the user. Reset when a pass succeeds.
    bool _pickingFailureReported = false;
};

}   // End of namespace
