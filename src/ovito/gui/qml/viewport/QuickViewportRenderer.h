// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/rendering/RendererService.h>
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/rendering/ObjectIdAllocator.h>

namespace Ovito {

/**
 * \brief Renders the frame graph of a viewport into the texture render target of a QQuickRhiItem.
 *
 * This class is the bridge between OVITO's renderer (SceneRenderer::Implementation) and the Qt Quick scene
 * graph. It plays the role that RenderThread plays for the classic frontend: it acts as the RendererService
 * that supplies the QRhi instance and the GPU resource caches to the renderer, but instead of owning a QRhi
 * instance of its own, it renders with the QRhi instance owned by the Qt Quick scene graph. As a consequence,
 * no render target or command buffer needs to be created and no readback of the rendered image is required.
 *
 * The renderer runs on the scene graph's rendering thread. The frame graph to render is handed over from the
 * GUI thread during QQuickRhiItemRenderer::synchronize(), i.e. while the GUI thread is blocked.
 */
class OVITO_GUIQML_EXPORT QuickViewportRenderer : public QQuickRhiItemRenderer, public RendererService
{
public:

    /// Constructor.
    explicit QuickViewportRenderer(QuickViewportItem* item);

    /// Destructor.
    ~QuickViewportRenderer() override;

    /// Returns the QRhi instance provided by the Qt Quick scene graph.
    QRhi* rhi() const override { return QQuickRhiItemRenderer::rhi(); }

    /// Returns the graphics API used by the Qt Quick scene graph.
    QRhi::Implementation graphicsApi() const override;

    /// Returns the cache for QRhi resources such as vertex buffers and textures.
    RendererResourceCache& rhiResourceCache() override { return *_resourceCache; }

    /// Returns the object ID allocator used for rendering and object picking.
    ObjectIdAllocator& objectIdAllocator() override { return _objectIdAllocator; }

    /// Loads a compiled .qsb shader from the Qt resource system.
    [[nodiscard]] QShader loadShader(const QString& resourcePath) override;

    /// Records a non-fatal warning encountered during rendering.
    void reportWarning(const QString& message) override;

    /// This service is always used from the scene graph's rendering thread.
    bool isRendererThread() const override { return true; }

    /// Returns the warning messages reported during the last rendered frame.
    const QStringList& warnings() const { return _warnings; }

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

    /// Cache for QRhi resources such as vertex buffers and textures.
    /// Declared before _implementation so that the renderer implementation (and with it all
    /// RendererResourceCache frames it holds) is destroyed first.
    std::shared_ptr<RendererResourceCache> _resourceCache = std::make_shared<RendererResourceCache>();

    /// Allocator for unique object IDs used in rendering and object picking.
    ObjectIdAllocator _objectIdAllocator;

    /// The frame graph currently rendered into the item.
    OORef<FrameGraph> _frameGraph;

    /// The renderer configuration belonging to the current frame graph.
    std::unique_ptr<SceneRenderer::Configuration> _rendererConfig;

    /// The renderer implementation used to render the frame graph.
    std::unique_ptr<SceneRenderer::Implementation> _implementation;

    /// Warning messages collected during the last rendered frame.
    QStringList _warnings;
};

}   // End of namespace
