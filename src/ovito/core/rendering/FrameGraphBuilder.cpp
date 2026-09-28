// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

/******************************************************************************
* Handles the status returned by a vis element's or overlay's render() method.
******************************************************************************/
static void handleRenderResult(ActiveObject* object, PipelineStatus&& status)
{
    // Pass error status codes to the exception handler.
    if(status.type() == PipelineStatus::Error)
        throw Exception(status.text());

    // In console mode, print warning messages to the terminal.
    if(status.type() == PipelineStatus::Warning && !status.text().isEmpty() && Application::runMode() != Application::AppMode) {
        if(dynamic_object_cast<DataVis>(object))
            qWarning() << "WARNING: Visual element" << object->objectTitle() << "reported:" << status.text();
        else if(dynamic_object_cast<ViewportOverlay>(object))
            qWarning() << "WARNING: Viewport layer" << object->objectTitle() << "reported:" << status.text();
        else
            OVITO_ASSERT(false);
    }

    // Display current status in the GUI.
    object->setStatus(std::move(status));
}

/******************************************************************************
* Handles an exception thrown by a vis element's or overlay's render() method.
******************************************************************************/
static void handleRenderException(const FrameGraph& fg, ActiveObject* object, Exception& ex)
{
    // Indicate error status in the GUI.
    object->setStatus(ex);

    // If the rendering of the element failed, interrupt entire rendering process in console mode; swallow exceptions in GUI mode.
    if(fg.stopOnPipelineError()) {
        if(dynamic_object_cast<DataVis>(object))
            throw ex.prependToMessage(QStringLiteral("Visual element '%1' reported an error: ").arg(object->objectTitle()));
        else if(dynamic_object_cast<ViewportOverlay>(object))
            throw ex.prependToMessage(QStringLiteral("Viewport layer '%1' reported an error: ").arg(object->objectTitle()));
        else
            OVITO_ASSERT(false);
    }
}

/******************************************************************************
* Builds the list of visible scene pipeline nodes to be rendered.
******************************************************************************/
static std::vector<OORef<SceneNode>> compilePipelineSceneNodesList(const Scene& scene, const Viewport* viewport)
{
    std::vector<OORef<SceneNode>> visiblePipelineSceneNodes;

    // Visit all pipelines in the current scene.
    scene.visitPipelines([&](SceneNode* sceneNode) {

        // Skip pipeline if it is hidden in the current viewport.
        if(viewport && sceneNode->isHiddenInViewport(viewport, false))
            return;

        // Skip pipeline if it is the view node of the viewport or if it is the target of the view node.
        if(viewport && viewport->viewNode() && (viewport->viewNode() == sceneNode || viewport->viewNode()->lookatTargetNode() == sceneNode))
            return;

        // Add the pipeline to the list of pipeline to be rendered.
        visiblePipelineSceneNodes.push_back(sceneNode);
    });

    return visiblePipelineSceneNodes;
}

/******************************************************************************
* Render the overlays/underlays of a viewport.
******************************************************************************/
static void renderOverlays(
    FrameGraph& fg,
    const Scene* scene,
    const QVector<OORef<ViewportOverlay>>& overlays,
    FrameGraph::RenderLayerType layerType,
    const QRect& logicalViewportRect,
    const QRect& physicalViewportRect,
    const ViewProjectionParameters& noninteractiveProjParams,
    std::vector<OORef<ViewportOverlay>>& asyncViewportLayers,
    std::vector<Future<PipelineStatus>>& asyncViewportLayersFutures)
{
    for(ViewportOverlay* layer : overlays) {
        if(layer->isEnabled()) {
            try {
                // Create the command group this overlay layer will add its rendering commands to.
                FrameGraph::RenderingCommandGroup& commandGroup = fg.addCommandGroup(layerType);

                // Let the layer do the rendering in an asynchronous way. The frame graph and scene are passed as owning
                // references so that an asynchronous coroutine can keep them alive across suspensions (see ARCHITECTURE.md §6).
                Future<PipelineStatus> future = layer->renderAsynchronous(OORef<FrameGraph>(&fg), commandGroup, logicalViewportRect, physicalViewportRect, noninteractiveProjParams, scene);

                // If the future is empty, fall back to the synchronous rendering method.
                if(!future) {
                    handleRenderResult(layer, layer->renderSynchronous(fg, commandGroup, logicalViewportRect, physicalViewportRect, noninteractiveProjParams, scene));
                }
                else {
                    // Collect all unfinished futures in a list to await them later.
                    if(!future.isFinished()) {
                        asyncViewportLayersFutures.push_back(std::move(future));
                        asyncViewportLayers.push_back(layer);
                    }
                    else {
                        handleRenderResult(layer, future.result());
                    }
                }
            }
            catch(Exception& ex) {
                handleRenderException(fg, layer, ex);
            }
        }
    }
}

/******************************************************************************
* Visits all vis elements of all data objects in the collection and lets them
* populate the frame graph.
******************************************************************************/
static void gatherVisElements(
    FrameGraph& fg,
    const DataObject* dataObj,
    const SceneNode* sceneNode,
    const PipelineFlowState& state,
    ConstDataObjectPath& dataObjectPath,
    std::vector<OORef<DataVis>>& asyncVisElements,
    std::vector<Future<PipelineStatus>>& asyncVisElementFutures)
{
    bool isOnStack = false;

    // Call all attached vis elements of the data object.
    for(DataVis* vis : dataObj->visElements()) {
        // Let the pipeline substitute the vis element with another one.
        vis = sceneNode->pipeline()->getReplacementVisElement(vis);
        if(vis->isEnabled()) {
            // Push the data object onto the stack.
            if(!isOnStack) {
                dataObjectPath.push_back(dataObj);
                isOnStack = true;
            }
            try {
                // Let the vis element do the rendering in an asynchronous way. The frame graph is passed as an owning OORef so that
                // any asynchronous sub-task the vis element launches can keep it alive (see ARCHITECTURE.md §6).
                Future<PipelineStatus> future = vis->renderAsynchronous(dataObjectPath, state, OORef<FrameGraph>(&fg), sceneNode);

                // If the future is empty, fall back to the synchronous rendering method.
                if(!future) {
                    handleRenderResult(vis, vis->renderSynchronous(dataObjectPath, state, fg, sceneNode));
                }
                else {
                    // Collect all unfinished futures in a list to await them later.
                    if(!future.isFinished()) {
                        asyncVisElementFutures.push_back(std::move(future));
                        asyncVisElements.push_back(vis);
                    }
                    else {
                        handleRenderResult(vis, future.result());
                    }
                }
            }
            catch(Exception& ex) {
                handleRenderException(fg, vis, ex);
            }
        }
    }

    // Recursively visit the sub-objects of the data object.
    dataObj->visitSubObjects([&](const DataObject* subObject) {
        // Push the next data object onto the stack.
        if(!isOnStack) {
            dataObjectPath.push_back(dataObj);
            isOnStack = true;
        }
        gatherVisElements(fg, subObject, sceneNode, state, dataObjectPath, asyncVisElements, asyncVisElementFutures);
    });

    // Pop the data object from the stack.
    if(isOnStack)
        dataObjectPath.pop_back();
}

/******************************************************************************
* Generates the frame graph contents for a scene.
******************************************************************************/
Future<void> FrameGraph::buildFromScene(OORef<Scene> scene, OORef<Viewport> viewport, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams)
{
    // The prologues of the vis element coroutines launched below run in this thread and rely on
    // main-thread-only operations (scene access, command group creation, lazy creation of the Qt
    // application object before touching QFont).
    OVITO_ASSERT(this_task::isMainThread());

    // Gather list of pipeline scene nodes to render.
    std::vector<OORef<SceneNode>> visiblePipelineSceneNodes = compilePipelineSceneNodesList(*scene, viewport);

    // Render viewport underlays and overlays.
	std::vector<OORef<ViewportOverlay>> asyncViewportLayers; // List of asynchronous viewport layers.
	std::vector<Future<PipelineStatus>> asyncViewportLayersFutures; // List of asynchronous viewport layer rendering tasks.
    if(!logicalViewportRect.isEmpty() && !physicalViewportRect.isEmpty()) {
        renderOverlays(*this, scene, viewport->underlays(), FrameGraph::UnderLayer, logicalViewportRect, physicalViewportRect, noninteractiveProjParams, asyncViewportLayers, asyncViewportLayersFutures);
        renderOverlays(*this, scene, viewport->overlays(), FrameGraph::OverLayer, logicalViewportRect, physicalViewportRect, noninteractiveProjParams, asyncViewportLayers, asyncViewportLayersFutures);
    }

    // Evaluate all visible pipelines in the scene to obtain their output data.
	std::vector<PipelineFlowState> pipelineResults;
    for(const auto& sceneNode : visiblePipelineSceneNodes) {
        try {
            // Request pipeline result.
            PipelineEvaluationResult pipelineResult = sceneNode->pipeline()->evaluatePipeline(PipelineEvaluationRequest(time(), stopOnPipelineError(), isInteractive()));

            // Flag the entire frame graph as preliminary if the pipeline output is preliminary.
            if(pipelineResult.evaluationTypes().testFlag(PipelineEvaluationResult::EvaluationType::Noninteractive) == false)
                setIsPreliminaryState(true);

            pipelineResults.push_back(co_await FutureAwaiter(DeferredObjectExecutor(this), std::move(pipelineResult).asFuture()));
        }
        catch(const Exception& ex) {
            if(stopOnPipelineError())
                throw;
            // In interactive mode, log the exception and continue rendering.
            ex.logError();
            pipelineResults.push_back({});
            continue;
        }
    }
    OVITO_ASSERT(pipelineResults.size() == visiblePipelineSceneNodes.size());

    // Visit all vis elements and let them populate the frame graph.
    std::vector<OORef<DataVis>> asyncVisElements; // List of asynchronous visual elements.
	std::vector<Future<PipelineStatus>> asyncVisElementFutures; // List of asynchronous visual element rendering tasks.
    auto sceneNode = visiblePipelineSceneNodes.begin();
    ConstDataObjectPath dataObjectPath;
    for(const PipelineFlowState& state : pipelineResults) {
        // Visit all vis elements of all data objects in the pipeline state.
        if(state)
            gatherVisElements(*this, state.data(), *sceneNode, state, dataObjectPath, asyncVisElements, asyncVisElementFutures);
        OVITO_ASSERT(dataObjectPath.empty());
        ++sceneNode;
    }

    // Wait for all visual elements to finish rendering.
    if(!asyncVisElementFutures.empty()) {
        // Wait for all asynchronous visual elements to finish rendering.
        asyncVisElementFutures = co_await FutureAwaiter(ObjectExecutor(this), whenAll(std::move(asyncVisElementFutures)));

        // Once all future results are available, handle them one by one.
        OVITO_ASSERT(asyncVisElementFutures.size() == asyncVisElements.size());
        for(size_t i = 0; i < asyncVisElementFutures.size(); ++i) {
            DataVis* vis = asyncVisElements[i];
            try {
                if(!asyncVisElementFutures[i].isCanceled())
                    handleRenderResult(vis, asyncVisElementFutures[i].result());
            }
            catch(Exception& ex) {
                handleRenderException(*this, vis, ex);
            }
        }
    }

    // Wait for all viewport overlays to finish rendering.
    if(!asyncViewportLayersFutures.empty()) {

        // Wait for all asynchronous viewport layers to finish rendering.
        asyncViewportLayersFutures = co_await FutureAwaiter(ObjectExecutor(this), whenAll(std::move(asyncViewportLayersFutures)));

        // Once all future results are available, handle them one by one.
        OVITO_ASSERT(asyncViewportLayersFutures.size() == asyncViewportLayers.size());
        for(size_t i = 0; i < asyncViewportLayersFutures.size(); ++i) {
            ViewportOverlay* overlay = asyncViewportLayers[i];
            try {
                if(!asyncViewportLayersFutures[i].isCanceled())
                    handleRenderResult(overlay, asyncViewportLayersFutures[i].result());
            }
            catch(Exception& ex) {
                handleRenderException(*this, overlay, ex);
            }
        }
    }

    // Compute combined scene bounding box.
    computeSceneBoundingBox();
}

}   // End of namespace
