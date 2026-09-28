// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/rendering/FrameGraphRenderPass.h>
#include "QuickRendererService.h"

namespace Ovito {

/**
 * \brief Renders the frame graph of a viewport into the texture render target of a QQuickRhiItem.
 *
 * This class is the bridge between OVITO's renderer (SceneRenderer::Implementation) and the Qt Quick scene
 * graph. It plays the role that a render target of RenderThread plays for the classic frontend, but instead of
 * rendering with a QRhi instance created on a thread of its own, it renders inside the QRhi frame the Qt Quick
 * scene graph has already started.
 *
 * The GPU resources it needs come from the QuickRendererService of the window the item belongs to, so that all
 * viewport items of a window share the QRhi instance and the caches (see QuickRendererService). The service is
 * picked up during scene graph synchronization, i.e. while the GUI thread is blocked, which is also when the
 * frame graph to render is handed over.
 *
 * The renderer runs on the scene graph's rendering thread.
 */
class OVITO_GUIQML_EXPORT QuickViewportRenderer : public QQuickRhiItemRenderer
{
public:

    /// Constructor.
    explicit QuickViewportRenderer(QuickViewportItem* item);

    /// Destructor.
    ~QuickViewportRenderer() override;

    /// Releases the GPU resources of this renderer, i.e. the renderer implementations of its render target.
    /// Called by QuickRendererService when the scene graph invalidates the QRhi instance they were created with.
    void releaseGraphicsResources();

protected:

    /// Is called when the GPU resources of the item need to be (re)created.
    void initialize(QRhiCommandBuffer* cb) override;

    /// Is called while the GUI thread is blocked, right before rendering. Picks up a newly generated frame graph.
    void synchronize(QQuickRhiItem* item) override;

    /// Is called before Qt Quick records its main render pass. Performs the actual rendering.
    void render(QRhiCommandBuffer* cb) override;

private:

    /// Renders the given frame graph into the item's texture render target.
    void renderFrameGraph(QRhiCommandBuffer* cb, FrameGraph& frameGraph, SceneRenderer::Configuration& rendererConfig);

    /// The QML item this renderer belongs to. Owned by the QML scene.
    QuickViewportItem* _item;

    /// The GPU resources of the window this item belongs to, shared with the other viewport items of the window.
    /// The service is owned by the window and dies with it, which can happen while a renderer that the scene graph
    /// has not destroyed yet still exists, so the pointer is a guarded one.
    QPointer<QuickRendererService> _service;

    /// The user interface the rendered frames belong to, taken from the item during synchronization so that the
    /// render pass does not have to touch objects living on the GUI thread.
    std::shared_ptr<UserInterface> _userInterface;

    /// The renderer implementations of this item's render target. Only the visual one is used, because picking
    /// is rendered asynchronously by the render thread (see QuickViewportWindow::renderPickingBuffer()).
    FrameGraphRenderPass::Implementations _implementations;

    /// The frame graph currently rendered into the item.
    OORef<FrameGraph> _frameGraph;

    /// The renderer configuration belonging to the current frame graph.
    std::unique_ptr<SceneRenderer::Configuration> _rendererConfig;
};

}   // End of namespace
