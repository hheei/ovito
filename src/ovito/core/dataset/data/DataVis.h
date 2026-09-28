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
#include <ovito/core/dataset/pipeline/ActiveObject.h>
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/**
 * \brief Abstract base class for display objects that are responsible
 *        for rendering DataObject-derived classes.
 */
class OVITO_CORE_EXPORT DataVis : public ActiveObject
{
    OVITO_CLASS(DataVis)

public:

    /// Constructor.
    using ActiveObject::ActiveObject;

    /// \brief Lets the vis element generate its rendering commands, possibly performing part of the work asynchronously.
    ///
    /// Rendering of a visual element is split into two virtual methods — renderAsynchronous() and renderSynchronous() —
    /// so that each subclass implements only the one that matches the nature of its rendering work. A subclass overrides
    /// exactly one of them:
    ///  - Override **renderAsynchronous()** when producing the rendering commands involves work that should not block the
    ///    main thread, e.g. generating geometry on a worker thread. This method is meant to be
    ///    implemented as a *coroutine*. The typical shape is: do the main-thread-only setup synchronously (create a
    ///    command group via frameGraph->addCommandGroup(), query the scene node's world transform), then
    ///    `co_await ExecutorAwaiter(ThreadPoolExecutor())` (or `co_await` another sub-task's future) to move the heavy
    ///    work off the main thread, and finally co_return the status.
    ///  - Override **renderSynchronous()** when the rendering is cheap and immediate, i.e. it runs entirely on the main
    ///    thread with no pipeline evaluation and no worker-thread offloading.
    ///
    /// The system always calls renderAsynchronous() first. Its default implementation returns an empty (invalid) future,
    /// which signals "asynchronous rendering not implemented"; the system then falls back to calling renderSynchronous().
    ///
    /// Caution: path and flowState are passed by const reference to avoid copying potentially large objects.
    /// They are only guaranteed to be valid up to the coroutine's first suspension point (the first co_await). A coroutine
    /// that needs them afterwards must copy what it needs into owning locals before suspending (e.g. extract the relevant
    /// DataObject as a DataOORef). frameGraph and sceneNode, in contrast, are owning references and remain valid throughout.
    ///
    /// \param path The data object to be rendered and its parent objects.
    /// \param flowState The pipeline evaluation results.
    /// \param frameGraph The output frame graph being generated.
    /// \param sceneNode The pipeline scene node that produced the data object.
    /// \return A future that will hold the status code of the rendering operation. This status is shown in the pipeline editor next to the visual element. An empty future means "not implemented — use renderSynchronous() instead".
    virtual Future<PipelineStatus> renderAsynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, OORef<FrameGraph> frameGraph, OORef<const SceneNode> sceneNode) { return {}; }

    /// \brief Lets the vis element generate its rendering commands synchronously on the main thread.
    ///
    /// This is the synchronous counterpart of renderAsynchronous() (see there for the rationale behind the split). A vis
    /// element implements this method instead of renderAsynchronous() when its rendering is cheap and immediate: no data
    /// pipeline evaluation and no worker-thread work.
    ///
    /// The system calls this method only as a fallback, after renderAsynchronous() has returned an empty future (the
    /// default). The default implementation does nothing and reports success.
    ///
    /// \param path The data object to be rendered and its parent objects.
    /// \param flowState The pipeline evaluation results.
    /// \param frameGraph The output frame graph being generated.
    /// \param sceneNode The pipeline scene node that produced the data object.
    /// \return A status code indicating the outcome of the rendering operation. This status is shown in the pipeline editor next to the visual element.
    virtual PipelineStatus renderSynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode) { return PipelineStatus::Success; }

    /// \brief Computes the view-dependent bounding box of the given data object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) = 0;

    /// \brief Indicates whether this visual element should be surrounded by a selection marker in the viewports when it is selected.
    /// \return \c true to let the system render a selection marker around the object when it is selected.
    ///
    /// The default implementation returns \c true.
    virtual bool showSelectionMarker() { return true; }

    /// \brief Returns all pipelines that produced this visualization element.
    /// \param onlyScenePipelines If true, pipelines which are currently not part of the scene are ignored.
    QSet<Pipeline*> pipelines(bool onlyScenePipelines) const;

    /// \brief Replaces this visual element with a shared visual element
    /// by telling all dependents to update their references.
    virtual void replaceWithSharedElement(DataVis* sharedVis) const;
};

}   // End of namespace
