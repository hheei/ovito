// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/TextPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/dataset/scene/ScenePreparation.h>
#include <ovito/core/utilities/concurrent/FutureWatcher.h>

namespace Ovito {

/**
 * \brief A viewport window provides the connection between the non-visual Viewport class and the GUI layer.
 */
class OVITO_CORE_EXPORT ViewportWindow : public QObject, public RefMaker, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT
    OVITO_CLASS(ViewportWindow)

public:

    /// Data structure returned by the ViewportWindow::pick() method,
    /// holding information on the object that has been picked in a viewport window.
    class OVITO_CORE_EXPORT PickResult
    {
    public:

        /// Constructor.
        PickResult(OORef<SceneNode> sceneNode, OORef<ObjectPickInfo> pickInfo, const Point3& hitLocation, quint32 subobjectId)
            : _sceneNode(std::move(sceneNode)), _pickInfo(std::move(pickInfo)), _hitLocation(hitLocation), _subobjectId(subobjectId) {}

        /// Returns the scene node that has been picked.
        const OORef<SceneNode>& sceneNode() const { return _sceneNode; }

        /// Returns the object-specific data at the pick location.
        const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

        /// Returns the coordinates of the hit point in world space.
        const Point3& hitLocation() const { return _hitLocation; }

        /// Returns the sub-object that was picked.
        quint32 subobjectId() const { return _subobjectId; }

    private:

        /// The scene node that was picked.
        OORef<SceneNode> _sceneNode;

        /// The object-specific data at the pick location.
        OORef<ObjectPickInfo> _pickInfo;

        /// The coordinates of the hit point in world space.
        Point3 _hitLocation;

        /// The subobject that was picked.
        quint32 _subobjectId = 0;
    };

public:

    /// Constructor.
    ViewportWindow();

    /// Associates this window with a viewport.
    void setViewport(Viewport* vp, UserInterface& ui);

    /// Returns the object responsible for evaluating all pipelines in the scene to prepare interactive rendering.
    ScenePreparation& scenePreparation() { OVITO_ASSERT(_scenePreparation); return *_scenePreparation; }

    /// Return the current 3D projection used to render the contents of the viewport window.
    const ViewProjectionParameters& projectionParams() const { return _projParams; }

    /// Returns whether the viewport window is using a perspective projection.
    bool isPerspectiveProjection() const { return projectionParams().isPerspective; }

    /// Indicates whether the window is currently shown or not.
    virtual bool isVisible() const = 0;

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() const = 0;

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() const = 0;

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() const = 0;

    /// Returns the visibility of the orientation indicator.
    bool isOrientationIndicatorVisible() const { return _showOrientationIndicator; }

    /// Controls the visibility of the orientation indicator.
    void setOrientationIndicatorVisible(bool visible) { _showOrientationIndicator = visible; }

    /// Returns whether the viewport caption is displayed.
    bool isViewportTitleVisible() const { return _showViewportTitle; }

    /// Sets whether the viewport caption is shown.
    void setViewportTitleVisible(bool visible) { _showViewportTitle = visible; }

    /// Indicates whether the mouse cursor is currently positioned inside the
    /// viewport window area that activates the context menu.
    bool cursorInContextMenuArea() const { return _cursorInContextMenuArea; }

    /// Sets a flag indicating whether the mouse cursor is currently located in the
    /// viewport window area that activates the context menu.
    void setCursorInContextMenuArea(bool flag);

    /// Returns the zone in the upper left corner of the viewport where the context menu can be activated by the user.
    const QRectF& contextMenuArea() const { return _contextMenuArea; }

    /// Determines the object located under the given mouse cursor position.
    virtual std::optional<PickResult> pick(const QPointF& pos) = 0;

    /// Returns the list of gizmos to render in the viewport.
    virtual std::vector<ViewportGizmo*> viewportGizmos() { return {}; }

    /// Sets the mouse cursor shape for the window.
    virtual void setCursor(const QCursor& cursor) {}

    /// Returns the current position of the mouse cursor relative to the viewport window.
    virtual QPoint getCurrentMousePos() const { return QPoint(); }

    /// Returns the icon image used for displaying non-fatal rendering warnings in the viewport window.
    virtual QImage warningIcon() const = 0;

    /// Updates the current rendering warning messages and icon area.
    /// Thread-safe: may be called from the render thread.
    void setWarnings(QStringList messages, QRectF iconArea) {
        QMutexLocker lock(&_warningsMutex);
        _warningMessages = std::move(messages);
        _warningIconArea = iconArea;
    }

    /// Returns the current non-fatal warning messages from the last rendered frame.
    /// Thread-safe: may be called from the GUI thread.
    QStringList currentWarnings() const {
        QMutexLocker lock(&_warningsMutex);
        return _warningMessages;
    }

    /// Returns the bounding rect of the warning icon overlay in device-independent pixels.
    /// Empty when there are no warnings. Thread-safe: may be called from the GUI thread.
    QRectF warningIconArea() const {
        QMutexLocker lock(&_warningsMutex);
        return _warningIconArea;
    }

    /// \brief Computes a point in the given coordinate system based on the given screen position and the current snapping settings.
    /// \param[in] screenPoint A point relative to the upper left corner of the viewport window.
    /// \param[out] snapPoint The resulting point in the coordinate system specified by \a snapSystem. If the method returned
    ///                       \c false then the value of this output variable is undefined.
    /// \param[in] snapSystem Specifies the coordinate system in which the snapping point should be determined.
    /// \return \c true if a snapping point has been found; \c false if no snapping point was found for the given screen position.
    bool snapPoint(const QPointF& screenPoint, Point3& snapPoint, const AffineTransformation& snapSystem) const;

    /// \brief Computes a point in the grid coordinate system based on a screen position and the current snap settings.
    /// \param[in] screenPoint A point relative to the upper left corner of the viewport window.
    /// \param[out] snapPoint The resulting snap point in the viewport's grid coordinate system. If the method returned
    ///                       \c false then the value of this output variable is undefined.
    /// \return \c true if a snapping point has been found; \c false if no snapping point was found for the given screen position.
    bool snapPoint(const QPointF& screenPoint, Point3& snapPoint) const;

    /// \brief Computes a ray in world space going through a pixel of the viewport window.
    /// \param screenPoint A screen point relative to the upper left corner of the viewport window.
    /// \return The ray that goes from the camera point through the specified pixel of the viewport window.
    Ray3 screenRay(const QPointF& screenPoint) const;

    /// Computes the geometry of the render preview frame, i.e., the cutout region of the interactive viewport window that
    /// will be visible in a rendered image. The returned rectangle is given in window coordinates.
    QRect previewFrameGeometry(DataSet* dataset, const QSize& windowSize) const;

    /// \brief Computes the intersection point of a ray going through a point in the
    ///        viewport plane with the construction grid plane.
    /// \param[in] viewportPosition A 2d point in viewport coordinates (in the range [-1,+1]).
    /// \param[out] intersectionPoint The coordinates of the intersection point in grid plane coordinates.
    ///                               The point can be transformed to world coordinates using the gridMatrix() transform.
    /// \param[in] epsilon This threshold value is used to test whether the ray is parallel to the grid plane.
    /// \return \c true if an intersection has been found; \c false if not.
    bool computeConstructionPlaneIntersection(const Point2& viewportPosition,
                                              Point3& intersectionPoint,
                                              FloatType epsilon = Ovito::epsilon);

    /// \brief Zooms to the extents of the given bounding box.
    void zoomToBox(const Box3& box);

    /// Returns how often this viewport window has been rendered during the current program session.
    int renderedFramesCounter() const { return _renderedFramesCounter; }

    /// Returns whether automatic updates of the viewport window are enabled.
    bool automaticUpdatesEnabled() const { return _automaticUpdatesEnabled; }

    /// Enables or disables automatic updates of the viewport window.
    void setAutomaticUpdatesEnabled(bool enabled) { _automaticUpdatesEnabled = enabled; }

public Q_SLOTS:

    /// Releases the renderer resources held by the viewport window and the renderer.
    virtual void releaseResources();

    /// Schedules a refresh for this window contents.
    /// Once control returns to the main event loop, and once the window is visible, the window will generate a new frame graph and repaint itself.
    virtual void requestRerender(bool isPreliminaryUpdate);

    /// Handles any pending update requests for this viewport window.
    /// This is also called to resume rendering after viewport updates were temporarily suspended.
    void handleUpdateRequests();

    /// Zooms to the extents of the scene.
    void zoomToSceneExtents();

    /// Zooms to the extents of the currently selected scene nodes.
    void zoomToSelectionExtents();

    /// Zooms to the extents of the scene once all scene pipelines have been computed.
    void zoomToSceneExtentsWhenReady();

Q_SIGNALS:

    /// Is emitted when the window is being hidden (e.g. minimized) or closed.
    void viewportWindowHidden();

    /// Is emitted when the scene is complete, i.e., after all pipelines
    /// have been fully evaluated at the current animation frame, and the frame graph has been rendered by the window.
    /// This signal is not emitted while the window is currently hidden.
    void frameCompleted();

    /// Is emitted by the window when a fatal error has occurred, which prevents the window
    /// from further displaying any content.
    void fatalError(const Exception& ex);

protected:

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted.
    virtual void aboutToBeDeleted() override;

    /// Renders the window contents after the frame graph was generated.
    virtual void renderFrameGraph(OORef<FrameGraph> frameGraph) = 0;

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Modifies the projection such that the render preview frame painted over the 3d scene exactly matches the true visible area.
    void adjustProjectionForRenderPreviewFrame(DataSet* dataset, ViewProjectionParameters& params, const QSize& windowSize);

    /// Render the axis tripod symbol in the corner of the viewport that indicates
    /// the coordinate system orientation.
    void renderOrientationIndicator(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QSize& windowSize);

    /// Paints the rectangular frame on top of the scene to indicate the visible image area.
    void renderPreviewFrame(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, DataSet* dataset, const QSize& windowSize);

    /// Renders the viewport caption text.
    QRectF renderViewportTitle(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup);

    /// Renders the visual representation of the modifiers in a pipeline.
    void renderPipelineModifiers(SceneNode* sceneNode, FrameGraph& frameGraph);

    /// Determines the range of the construction grid to display.
    std::tuple<FloatType, Box2I> determineConstructionGridRange();

    /// Renders the construction grid in a viewport.
    void renderConstructionGrid(FrameGraph& frameGraph);

    /// Returns the asynchronous task generating the frame graph for the next frame.
    FutureWatcher<Future<OORef<FrameGraph>>>& frameGraphGenerationWatcher() { return _frameGraphGenerationWatcher; }

    /// A coroutine that generates a frame graph for the current viewport contents without rendering it.
    ///
    /// Besides the interactive rendering path, subclasses can use this to obtain a frame graph for offscreen
    /// rendering, e.g. to render an object picking pass in the background. The caller takes ownership of the
    /// returned frame graph and must not submit it to more than one renderer at a time.
    Future<OORef<FrameGraph>> generateFrameGraph();

private Q_SLOTS:

    /// Handles the completion of the asynchronous frame graph generation task.
    void frameGraphReady();

private:

    /// The viewport associated with this window.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<Viewport>, viewport, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_NO_UNDO);

    /// The interactive scene renderer to use for this window.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SceneRenderer>, sceneRenderer, setSceneRenderer, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_NO_UNDO);

    /// Counts how often this viewport window has been rendered during the current program session.
    int _renderedFramesCounter = 0;

    /// Controls whether automatic updates of the viewport window are enabled.
    bool _automaticUpdatesEnabled = true;

    /// Object responsible for evaluating all pipelines in the scene to prepare interactive rendering.
    OORef<ScenePreparation> _scenePreparation;

    /// Indicates that the program has requested an update of this viewport window.
    bool _updateNeeded = false;

    /// For a short delay of viewport updates during preliminary scene updates.
    QBasicTimer _preliminaryUpdateTimer;

    /// The asynchronous task generating the frame graph for the next frame.
    FutureWatcher<Future<OORef<FrameGraph>>> _frameGraphGenerationWatcher;

    /// Controls the visibility of the orientation indicator.
    bool _showOrientationIndicator = true;

    /// Controls the visibility of the viewport caption.
    bool _showViewportTitle = true;

    /// The zone in the upper left corner of the viewport window where
    /// the context menu can be activated by the user.
    QRectF _contextMenuArea;

    /// Indicates that the mouse cursor is currently positioned inside the
    /// viewport window area that activates the context menu.
    bool _cursorInContextMenuArea = false;

    /// The current 3D projection for rendering the contents of the viewport window.
    ViewProjectionParameters _projParams;

    /// Thread-safe warning state: written by the render thread, read by the GUI thread.
    mutable QMutex _warningsMutex;
    QStringList _warningMessages;
    QRectF _warningIconArea;
};

}   // End of namespace

#include <ovito/core/viewport/Viewport.h>
