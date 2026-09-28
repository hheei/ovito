// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RendererService.h>
#include <functional>

namespace Ovito {

/**
 * \brief Renders a frame graph into a QRhi render target.
 *
 * OVITO executes a frame graph with the pass sequence implemented here. That sequence is the same for every
 * render target - the swap chain of a classic viewport window, an offscreen target, the picking target, or the
 * texture render target of a Qt Quick scene graph item - and it has to be, because the renderer implementations
 * depend on the order of the calls: resource uploads are prepared before beginPass(), pre-passes run before the
 * scene pass, and the post-process pass of an outline renderer runs after it.
 *
 * The caller provides the service that owns the GPU resources (\ref RendererService), the render target, the
 * frame graph and the renderer implementations it keeps per render target. Everything that is specific to the
 * target - a watermark, an indicator for warnings - is added through the callbacks of \ref Arguments, so that
 * this class does not need to know about the views the frame graph is rendered for.
 *
 * This class only records the passes; the caller owns the QRhi frame they are part of (an offscreen frame of
 * RenderThread, or the frame the Qt Quick scene graph has started).
 */
class OVITO_CORE_EXPORT FrameGraphRenderPass
{
public:

    /// The renderer implementations of one render target, which the caller keeps alive between the passes.
    struct Implementations
    {
        /// The implementation that renders the visible scene.
        std::unique_ptr<SceneRenderer::Implementation> visual;

        /// The implementation that renders object IDs for picking. Only some renderers provide one.
        std::unique_ptr<SceneRenderer::Implementation> picking;

        /// Returns the implementation that renders the given kind of pass.
        SceneRenderer::Implementation* get(bool isPickingPass) const { return isPickingPass ? picking.get() : visual.get(); }

        /// Releases both implementations, e.g. because the render pass descriptor of the target has changed and
        /// pipelines created for it are not compatible anymore.
        void reset() { visual.reset(); picking.reset(); }

        /// Indicates whether any implementation has been created yet.
        explicit operator bool() const { return visual || picking; }
    };

    /// Uploads additional data together with the scene pass.
    using ResourceUpdatesFunction = std::function<void(QRhiResourceUpdateBatch* updates)>;

    /// Records additional content at the end of the scene pass, e.g. an overlay drawn on top of the frame graph.
    using PassContentFunction = std::function<void(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget)>;

    /// Describes one pass.
    struct Arguments
    {
        /// The service that supplies the QRhi instance and the GPU resource caches.
        RendererService& service;

        /// The command buffer the pass is recorded into.
        QRhiCommandBuffer& cb;

        /// The render target the pass writes to.
        QRhiRenderTarget& renderTarget;

        /// The frame graph to execute. It is finalized by this pass and must not be submitted twice.
        FrameGraph& frameGraph;

        /// Describes the scene renderer that built the frame graph.
        const SceneRenderer::Configuration& configuration;

        /// The renderer implementations of this render target. The pass replaces an implementation if the
        /// configuration asks for it, e.g. because the frame graph was built by a different renderer.
        Implementations& implementations;

        /// Reports the progress of progressive refinement rendering, if the renderer performs any.
        TaskProgress& progress = TaskProgress::Ignore;

        /// Renders object IDs instead of the visible scene.
        bool isPickingPass = false;

        /// The picking map to fill while rendering a picking pass.
        ObjectPickingMap* pickingMap = nullptr;

        /// The iteration of a progressive refinement pass; 0 for a complete pass.
        int refinementIteration = 0;

        /// Additional updates to submit together with the scene pass, e.g. a watermark texture.
        ResourceUpdatesFunction prepareScenePass;

        /// Additional content to record at the end of the scene pass. Called once for the scene pass and, when
        /// an intermediate target is in use, once more for the final target after post-processing.
        PassContentFunction extendScenePass;
    };

    /// Executes one pass. If the scene renderer provides no implementation for the pass, a cleared pass is
    /// recorded instead (the target is left in a defined state either way) and a warning is reported through the
    /// service. Renderers that do not support picking provide no implementation for picking passes, which is not
    /// an error and stays silent.
    static void execute(const Arguments& args);
};

}   // End of namespace
