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
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/rendering/FrameGraph.h>

namespace Ovito {

/**
 * \brief Abstract base class for all viewport layer types.
 */
class OVITO_CORE_EXPORT ViewportOverlay : public ActiveObject
{
public:

    /// A meta-class for viewport layers (i.e. classes derived from ViewportOverlay).
    class OVITO_CORE_EXPORT OOMetaClass : public ActiveObject::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ActiveObject::OOMetaClass::OOMetaClass;

        /// Returns the category under which the layer will be displayed in the drop-down list box.
        virtual QString viewportOverlayCategory() const;
    };

    OVITO_CLASS_META(ViewportOverlay, OOMetaClass);

public:

    /// This virtual method gets called when the overlay is being newly attached to a viewport.
    virtual void initializeOverlay(Viewport* viewport);

    /// \brief Lets the overlay generate its rendering commands, possibly performing part of the work asynchronously.
    ///
    /// As for visual elements (see DataVis::renderAsynchronous()), overlay rendering is split into an asynchronous and a
    /// synchronous variant so that each subclass implements only the one matching the nature of its work. A subclass
    /// overrides exactly one of renderAsynchronous() and renderSynchronous().
    ///
    /// Override **renderAsynchronous()** when painting the overlay involves work that should not block the main thread —
    /// most commonly evaluating a data pipeline (e.g. a color legend driven by a color-coding modifier) or running a
    /// user-defined Python script. Implement it as a *coroutine*: do the main-thread-only setup synchronously, then
    /// `co_await` a pipeline-evaluation future (or `co_await ExecutorAwaiter(ThreadPoolExecutor())`), and co_return the
    /// status.
    ///
    /// Caution: logicalViewportRect, physicalViewportRect and noninteractiveProjParams are passed by value precisely so a
    /// coroutine may safely use them after a suspension point (they are stored in the coroutine frame). commandGroup is a
    /// borrowed reference that remains valid because frameGraph keeps the owning frame graph alive.
    ///
    /// \param frameGraph The output frame graph being generated.
    /// \param commandGroup The command group (under/over layer) this overlay adds its rendering commands to.
    /// \param logicalViewportRect The overlay's drawing rectangle in logical (device-independent) coordinates.
    /// \param physicalViewportRect The overlay's drawing rectangle in physical (device pixel) coordinates.
    /// \param noninteractiveProjParams The view projection parameters of the rendered viewport.
    /// \param scene The scene being rendered (used to locate a source pipeline if none is set explicitly).
    /// \return A future holding the status code of the rendering operation. An empty future means "not implemented — use renderSynchronous() instead".
    virtual Future<PipelineStatus> renderAsynchronous(OORef<FrameGraph> frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, QRect logicalViewportRect, QRect physicalViewportRect, ViewProjectionParameters noninteractiveProjParams, OORef<const Scene> scene) { return {}; }

    /// \brief Lets the overlay generate its rendering commands synchronously on the main thread.
    ///
    /// This is the synchronous counterpart of renderAsynchronous() (see there for the rationale behind the split). An
    /// overlay implements this method instead of renderAsynchronous() when its painting is cheap and immediate, i.e. it
    /// needs no pipeline evaluation and no worker-thread work. The system calls this method only as a fallback, after
    /// renderAsynchronous() has returned an empty future (the default). The default implementation reports success.
    ///
    /// \param frameGraph The output frame graph being generated.
    /// \param commandGroup The command group (under/over layer) this overlay adds its rendering commands to.
    /// \param logicalViewportRect The overlay's drawing rectangle in logical (device-independent) coordinates.
    /// \param physicalViewportRect The overlay's drawing rectangle in physical (device pixel) coordinates.
    /// \param noninteractiveProjParams The view projection parameters of the rendered viewport.
    /// \param scene The scene being rendered (used to locate a source pipeline if none is set explicitly).
    /// \return A status code indicating the outcome of the rendering operation.
    virtual PipelineStatus renderSynchronous(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene) { return PipelineStatus::Success; }

    /// Moves the position of the layer in the viewport by the given amount,
    /// which is specified as a fraction of the viewport render size.
    ///
    /// Layer implementations should override this method if they support positioning.
    /// The default method implementation does nothing.
    virtual void moveLayerInViewport(const Vector2& delta) {}

    /// Helper method that checks whether the given Qt alignment value contains exactly one horizontal and one vertical alignment flag.
    void checkAlignmentParameterValue(int alignment) const;

    /// Informs the overlay that a new scene node has been inserted into the scene.
    virtual void sceneNodeAdded(SceneNode* node);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Asks the object to register internal object references that will be saved to a data stream.
    virtual void registerObjectReferencesForSerialization(ObjectSaveStream& stream, const RefTarget* deltaReferenceObject) const override;

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// The pipeline generating the data that is being used by the overlay (optional).
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Pipeline>, pipeline, setPipeline, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_DONT_SERIALIZE);
};

}   // End of namespace
