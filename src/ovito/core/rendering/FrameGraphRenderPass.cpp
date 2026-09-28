// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "FrameGraphRenderPass.h"

namespace Ovito {

/******************************************************************************
* Sets the viewport and the scissor rectangle to cover the entire render target.
******************************************************************************/
static void setFullViewport(QRhiCommandBuffer& cb, const QSize& pixelSize)
{
    cb.setViewport(QRhiViewport(0, 0, float(pixelSize.width()), float(pixelSize.height())));
    // Workaround for Qt's Vulkan backend: setViewport() does not touch the scissor rectangle if no graphics
    // pipeline is currently bound, which would leave it in the state a previous pass (e.g. one for a smaller
    // intermediate target) left behind.
    cb.setScissor(QRhiScissor(0, 0, pixelSize.width(), pixelSize.height()));
}

/******************************************************************************
* Renders a frame graph into a render target.
******************************************************************************/
void FrameGraphRenderPass::execute(const Arguments& args)
{
    RendererService& service = args.service;
    QRhiCommandBuffer& cb = args.cb;
    QRhiRenderTarget& renderTarget = args.renderTarget;

    // Preprocess the frame graph:
    //  - replace all text primitives with image primitives,
    //  - compute the effective wireframe line widths from the current DPI.
    args.frameGraph.finalizeForRendering();

    // Create or update the renderer implementation of this pass. The configuration decides whether the existing
    // implementation can be updated in place or has to be replaced, e.g. because the frame graph was built by a
    // different renderer than the previous one.
    if(args.isPickingPass)
        args.implementations.picking = args.configuration.createImplementationForPicking(&service, std::move(args.implementations.picking));
    else
        args.implementations.visual = args.configuration.createImplementationForVisual(&service, std::move(args.implementations.visual));

    SceneRenderer::Implementation* implementation = args.implementations.get(args.isPickingPass);
    if(!implementation) {
        // Renderers that do not support picking provide no implementation for a picking pass. That is not an
        // error, it only means that nothing can be picked, so the pass clears the target and reports zero hits
        // to the caller. A missing implementation for a visual pass is a real defect of the renderer.
        if(!args.isPickingPass)
            service.reportWarning(QObject::tr("The scene renderer does not provide an implementation for rendering."));
        const QSize pixelSize = renderTarget.pixelSize();
        cb.beginPass(&renderTarget, QColor(0, 0, 0, 0), { 1.0f, 0 });
        setFullViewport(cb, pixelSize);
        cb.endPass();
        return;
    }

    // A progressive refinement renderer repeats the pass, and the caller's progress reporting follows it.
    bool finalRefinementPass = false;
    if(!args.isPickingPass) {
        const int totalRefinementIterations = implementation->totalRefinementIterations(args.frameGraph, args.configuration);
        if(totalRefinementIterations > 0) {
            OVITO_ASSERT(args.refinementIteration < totalRefinementIterations);
            finalRefinementPass = (args.refinementIteration + 1 >= totalRefinementIterations);
            if(args.refinementIteration == 0)
                args.progress.beginSubSteps(totalRefinementIterations);
            else
                args.progress.nextSubStep();
        }
    }

    // The render size is the size of the final render target.
    const QSize pixelSize = renderTarget.pixelSize();

    // Build the draw calls of the frame graph.
    implementation->renderFrame(args.frameGraph, args.configuration, pixelSize, args.progress, args.isPickingPass,
        args.refinementIteration, args.isPickingPass ? args.pickingMap : nullptr);

    // Some renderers (e.g. the outline renderer) render the scene into an intermediate target and post-process
    // it. Intermediate targets are only used for visual passes. This has to happen before the resource updates
    // are prepared, so that pipelines are created for the render pass the frame graph is finally rendered into.
    QRhiRenderTarget* sceneTarget = &renderTarget;
    if(!args.isPickingPass) {
        if(QRhiRenderTarget* intermediate = implementation->prepareIntermediateTarget(renderTarget.renderPassDescriptor(), pixelSize))
            sceneTarget = intermediate;
    }
    const bool useIntermediateTarget = (sceneTarget != &renderTarget);

    // Upload vertex, texture and uniform buffer data. This must happen before beginPass().
    QRhiResourceUpdateBatch* resourceUpdates = service.rhi()->nextResourceUpdateBatch();
    implementation->prepareResourceUpdates(sceneTarget, resourceUpdates, pixelSize, args.isPickingPass, args.frameGraph);
    if(args.prepareScenePass)
        args.prepareScenePass(resourceUpdates);

    // Renderer-specific pre-passes, e.g. GPU depth sorting or the accumulation passes of order-independent
    // transparency. The pending batch is passed by pointer, so that a pre-pass which needs the uploaded data
    // can submit it in a pass of its own.
    implementation->performPrePasses(&cb, pixelSize, args.isPickingPass, resourceUpdates);

    const QColor clearColor = args.isPickingPass ? QColor(0, 0, 0, 0) : static_cast<QColor>(args.frameGraph.clearColor());

    // Record the scene pass, committing the pending resource updates with it. While an intermediate target is in
    // use, the frame graph's overlay is skipped here and drawn into the final target after post-processing, so
    // that it appears on top of all effects.
    cb.beginPass(sceneTarget, clearColor, { 1.0f, 0 }, resourceUpdates);
    setFullViewport(cb, pixelSize);
    implementation->compositeInPass(&cb, sceneTarget, args.isPickingPass, useIntermediateTarget);
    if(args.extendScenePass)
        args.extendScenePass(&cb, sceneTarget);
    cb.endPass();

    // Post-process the intermediate target and draw the overlay into the final target.
    if(useIntermediateTarget) {
        // The post-process updates are built before beginPass(); D3D11 rejects resourceUpdate() calls while a
        // render pass is active.
        QRhiResourceUpdateBatch* postProcessUpdates = implementation->preparePostProcess(&cb);
        cb.beginPass(&renderTarget, clearColor, { 1.0f, 0 }, postProcessUpdates);
        setFullViewport(cb, pixelSize);
        implementation->runPostProcess(&cb, &renderTarget);
        implementation->renderOverLayerOnly(&cb, &renderTarget);
        if(args.extendScenePass)
            args.extendScenePass(&cb, &renderTarget);
        cb.endPass();
    }

    // The last iteration completes the progressive refinement.
    if(finalRefinementPass)
        args.progress.endSubSteps();
}

}   // End of namespace
