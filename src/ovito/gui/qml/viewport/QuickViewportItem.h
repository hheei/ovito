// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/FrameGraph.h>

namespace Ovito {

/**
 * \brief A Qt Quick item that displays the contents of a viewport of the current dataset.
 *
 * The item owns a QuickViewportWindow, which generates the frame graphs and handles the user input, and a
 * QuickViewportRenderer, which renders the frame graph on the Qt Quick render thread using the QRhi instance
 * of the Qt Quick scene graph.
 *
 * Handing a new frame graph to the renderer happens during the scene graph synchronization phase
 * (QQuickRhiItemRenderer::synchronize()), i.e. while the GUI thread is blocked, which makes the transfer race-free.
 */
class OVITO_GUIQML_EXPORT QuickViewportItem : public QQuickRhiItem
{
    Q_OBJECT

public:

    /// Constructor.
    explicit QuickViewportItem(QQuickItem* parent = nullptr);

    /// Destructor.
    ~QuickViewportItem() override;

    /// Associates this item with a viewport of the current dataset.
    /// The renderer object only provides the user-configurable rendering settings; the actual rendering is
    /// performed by the QuickViewportRenderer on the Qt Quick render thread.
    void initializeWindow(Viewport* viewport, UserInterface& userInterface, SceneRenderer* renderer);

    /// Returns the viewport window driving this item.
    QuickViewportWindow* viewportWindow() const { return _viewportWindow; }

    /// Hands a newly generated frame graph over to the render thread.
    /// Must be called on the GUI thread.
    void submitFrameGraph(OORef<FrameGraph> frameGraph, std::unique_ptr<SceneRenderer::Configuration> rendererConfig);

    /// Discards the frame graph waiting to be rendered. Must be called on the GUI thread.
    void releaseFrameGraph();

    /// Takes the pending frame graph and renderer configuration (if any). Called on the render thread
    /// during scene graph synchronization, i.e. while the GUI thread is blocked.
    void takePendingFrame(OORef<FrameGraph>& frameGraph, std::unique_ptr<SceneRenderer::Configuration>& rendererConfig);

Q_SIGNALS:

    /// Is emitted after the render thread has rendered a frame graph into this item.
    /// Auto-connections to objects living on the GUI thread are queued automatically.
    void frameRendered();

protected:

    /// Creates the renderer object that implements the rendering logic on the Qt Quick render thread.
    QQuickRhiItemRenderer* createRenderer() override;

    /// Is called when the geometry of the item has changed.
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

    /// Is called when a property of the item has changed.
    void itemChange(ItemChange change, const ItemChangeData& value) override;

    /// Is called when the scene graph resources of this item are released.
    void releaseResources() override;

    /// Handles double click events.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// Handles mouse press events.
    void mousePressEvent(QMouseEvent* event) override;

    /// Handles mouse release events.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// Handles mouse move events.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// Handles mouse wheel events.
    void wheelEvent(QWheelEvent* event) override;

    /// Handles key-press events.
    void keyPressEvent(QKeyEvent* event) override;

    /// Is called when the item looses the input focus.
    void focusOutEvent(QFocusEvent* event) override;

    /// Is called when the mouse cursor leaves the item.
    void hoverLeaveEvent(QHoverEvent* event) override;

private:

    /// The viewport window that generates the frame graphs for this item and handles the viewport input.
    OORef<QuickViewportWindow> _viewportWindow;

    /// The frame graph that is waiting to be rendered by the Qt Quick render thread.
    OORef<FrameGraph> _pendingFrameGraph;

    /// The renderer configuration that belongs to the pending frame graph.
    std::unique_ptr<SceneRenderer::Configuration> _pendingRendererConfig;

    /// Indicates whether the item has been reported as visible to the viewport window.
    bool _shown = false;
};

}   // End of namespace
