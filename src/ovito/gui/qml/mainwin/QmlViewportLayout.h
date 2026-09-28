// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/core/viewport/ViewportLayout.h>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QRectF>

namespace Ovito {

class QmlMainWindowUI;
class Viewport;
class ViewportConfiguration;

/**
 * \brief One pane of the workbench's viewport area.
 *
 * The pane carries the layout data of a single viewport - where it is, whether it is the active one and whether it is
 * currently maximized - to the QML scene. The QML delegate of the pane creates the viewport item that renders the
 * OVITO scene; the pane objects themselves are owned by the QmlViewportLayout model.
 *
 * Pane objects survive a window resize or a splitter drag, so that the QML delegate (and the viewport item holding the
 * GPU resources) does not have to be rebuilt while the user drags a splitter.
 */
class OVITO_GUIQML_EXPORT QmlViewportPane : public QObject
{
    Q_OBJECT

    /// The index of the viewport within the current viewport configuration.
    Q_PROPERTY(int viewportIndex READ viewportIndex NOTIFY viewportIndexChanged)
    /// Position and size of the pane within the workbench's viewport area.
    Q_PROPERTY(qreal x READ x NOTIFY rectChanged)
    Q_PROPERTY(qreal y READ y NOTIFY rectChanged)
    Q_PROPERTY(qreal width READ width NOTIFY rectChanged)
    Q_PROPERTY(qreal height READ height NOTIFY rectChanged)
    /// Whether the pane displays the active viewport.
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    /// Whether the pane's viewport is the maximized one.
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    /// Whether the pane is part of the current layout. A pane of a hidden viewport keeps its viewport item alive but
    /// releases its GPU resources.
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    /// Whether this pane can be maximized, which is not possible if the layout consists of a single viewport only.
    Q_PROPERTY(bool maximizable READ isMaximizable NOTIFY maximizableChanged)

public:

    /// Constructor.
    QmlViewportPane(int viewportIndex, OORef<Viewport> viewport, QObject* parent = nullptr);

    /// Returns the index of the viewport within the current viewport configuration.
    int viewportIndex() const { return _viewportIndex; }

    /// Returns the viewport displayed in this pane.
    Viewport* viewport() const { return _viewport.get(); }

    /// Returns the geometry of the pane.
    qreal x() const { return _rect.x(); }
    qreal y() const { return _rect.y(); }
    qreal width() const { return _rect.width(); }
    qreal height() const { return _rect.height(); }

    /// Returns the state of the pane.
    bool isActive() const { return _active; }
    bool isMaximized() const { return _maximized; }
    bool isVisible() const { return _visible; }
    bool isMaximizable() const { return _maximizable; }

    /// Sets the geometry of the pane.
    void setGeometry(const QRectF& rect);

    /// Sets the state of the pane.
    void setState(int viewportIndex, bool active, bool maximized, bool visible, bool maximizable);

Q_SIGNALS:

    void viewportIndexChanged();
    void rectChanged();
    void activeChanged();
    void maximizedChanged();
    void visibleChanged();
    void maximizableChanged();

private:

    /// Index of the viewport within the current viewport configuration.
    int _viewportIndex;

    /// The viewport displayed in this pane.
    OORef<Viewport> _viewport;

    /// Position and size of the pane within the viewport area.
    QRectF _rect;

    /// State of the pane.
    bool _active = false;
    bool _maximized = false;
    bool _visible = true;
    bool _maximizable = false;
};

/**
 * \brief The handle between two panes of the workbench's viewport area.
 *
 * Dragging the handle changes the relative sizes of the two neighboring panes. The change is recorded as a single
 * undoable operation, so one drag corresponds to one undo step.
 */
class OVITO_GUIQML_EXPORT QmlViewportSplitter : public QObject
{
    Q_OBJECT

    /// Position and size of the handle within the workbench's viewport area.
    Q_PROPERTY(qreal x READ x NOTIFY rectChanged)
    Q_PROPERTY(qreal y READ y NOTIFY rectChanged)
    Q_PROPERTY(qreal width READ width NOTIFY rectChanged)
    Q_PROPERTY(qreal height READ height NOTIFY rectChanged)
    /// Whether the handle splits the pane area horizontally, i.e. whether it is a vertical handle that is dragged
    /// along the x axis.
    Q_PROPERTY(bool horizontal READ isHorizontal NOTIFY orientationChanged)

public:

    /// Constructor.
    QmlViewportSplitter(QObject* parent = nullptr);

    /// Returns the geometry of the handle.
    qreal x() const { return _rect.x(); }
    qreal y() const { return _rect.y(); }
    qreal width() const { return _rect.width(); }
    qreal height() const { return _rect.height(); }

    /// Returns the orientation of the handle.
    bool isHorizontal() const { return _horizontal; }

    /// Sets the geometry of the handle.
    void setGeometry(const QRectF& rect, bool horizontal);

    /// Returns the layout cell this handle belongs to.
    ViewportLayoutCell* cell() const { return _cell; }

    /// Returns the index of the child cell the handle precedes (it lies between this child and the next one).
    int childIndex() const { return _childIndex; }

    /// Sets which layout cell and which pair of children this handle resizes.
    void setLayoutCell(ViewportLayoutCell* cell, int childIndex);

    /// Passes the data a drag of this handle needs: the rectangle the cell occupies and how many weight units one
    /// pixel of pointer movement corresponds to.
    void setDragParameters(const QRectF& cellRect, qreal dragFactor);

    /// Returns the rectangle the layout cell of this handle occupies.
    const QRectF& cellRect() const { return _cellRect; }

    /// Returns the number of weight units per pixel along the drag axis.
    qreal dragFactor() const { return _dragFactor; }

Q_SIGNALS:

    void rectChanged();
    void orientationChanged();

private:

    /// The layout cell that is split by this handle.
    ViewportLayoutCell* _cell = nullptr;

    /// Index of the child cell preceding the handle.
    int _childIndex = -1;

    /// Position and size of the handle within the viewport area.
    QRectF _rect;

    /// Rectangle the layout cell of this handle occupies.
    QRectF _cellRect;

    /// Number of weight units per pixel along the drag axis.
    qreal _dragFactor = 0;

    /// Orientation of the handle.
    bool _horizontal = true;
};

/**
 * \brief The layout of the workbench's viewport area.
 *
 * The model mirrors the viewport layout tree of the current dataset (`ViewportConfiguration::layoutRootCell()`) and
 * tells the QML scene where the viewports are (`panes`) and where the draggable handles between them are (`splitters`).
 * It is the Qt Quick counterpart of the classic frontend's `ViewportsPanel`, which lays the same tree out with widgets;
 * the same rules apply, in particular the minimum weight of 0.1 that keeps a pane from collapsing while dragging.
 *
 * Layout changes are undoable: a drag of a splitter is one transaction, and the "resize evenly" action of a handle is
 * one transaction as well. Maximizing a viewport is not undoable, matching the classic frontend (the maximized viewport
 * is not part of the undo stack, it is remembered in the application settings instead).
 */
class OVITO_GUIQML_EXPORT QmlViewportLayout : public QObject
{
    Q_OBJECT

    /// The panes of the viewport area. The list changes only when the layout structure changes, not when the user
    /// resizes a pane or the window.
    Q_PROPERTY(QVariantList panes READ panes NOTIFY panesChanged)

    /// The handles between the panes of the viewport area.
    Q_PROPERTY(QVariantList splitters READ splitters NOTIFY splittersChanged)

    /// Index of the active viewport, i.e. of the pane the viewport commands apply to. -1 if there is none.
    Q_PROPERTY(int activeViewportIndex READ activeViewportIndex NOTIFY activeViewportIndexChanged)

    /// Whether a viewport can be maximized, which requires a layout with more than one pane.
    Q_PROPERTY(bool maximizable READ isMaximizable NOTIFY maximizableChanged)

public:

    /// Constructor.
    explicit QmlViewportLayout(QmlMainWindowUI& ui, QObject* parent = nullptr);

    /// Returns the panes of the viewport area.
    QVariantList panes() const;

    /// Returns the handles between the panes of the viewport area.
    QVariantList splitters() const;

    /// Informs the model about the size of the area the viewports are laid out in. Must be called by the QML scene
    /// whenever that area changes.
    Q_INVOKABLE void setPanelSize(qreal width, qreal height);

    /// Returns the index of the active viewport, or -1 if there is none.
    int activeViewportIndex() const { return _activeViewportIndex; }

    /// Returns whether a viewport can be maximized.
    bool isMaximizable() const { return _maximizable; }

    /// Starts dragging the handle with the given index. The position is the pointer position in the coordinate system
    /// of the viewport area, from which the model derives how far the handle has been moved.
    /// \return False if the handle does not exist or if a drag is already in progress.
    Q_INVOKABLE bool beginSplitterDrag(int splitterIndex, qreal mouseX, qreal mouseY);

    /// Moves the handle that is currently being dragged to the given pointer position.
    Q_INVOKABLE void dragSplitter(qreal mouseX, qreal mouseY);

    /// Finishes the drag of the current handle and keeps the new pane sizes.
    Q_INVOKABLE void endSplitterDrag();

    /// Aborts the drag of the current handle and restores the pane sizes it had before the drag started.
    Q_INVOKABLE void cancelSplitterDrag();

    /// Restores the pane sizes on both sides of the handle, undoing any previous resizing.
    Q_INVOKABLE void resetSplitter(int splitterIndex);

    /// Maximizes the given viewport, or restores the former layout if that viewport is already the maximized one.
    Q_INVOKABLE void toggleMaximize(int viewportIndex);

Q_SIGNALS:

    void panesChanged();
    void splittersChanged();
    void activeViewportIndexChanged();
    void maximizableChanged();

private:

    /// The pane geometry of one viewport of the layout tree.
    struct PaneGeometry {
        Viewport* viewport;
        QRectF rect;
        bool visible;
    };

    /// The geometry of one handle between two child cells of the layout tree.
    struct SplitterGeometry {
        ViewportLayoutCell* cell;
        int childIndex;
        bool horizontal;
        QRectF rect;
        QRectF cellRect;
        qreal dragFactor;
    };

    /// Lays the given layout cell out in the given rectangle and collects the pane and handle geometries.
    void collectLayout(ViewportLayoutCell* cell, const QRectF& rect, std::vector<PaneGeometry>& panes, std::vector<SplitterGeometry>& splitters) const;

    /// Rebuilds the panes and handles from the viewport configuration of the current dataset.
    void updateLayout();

    /// Retires the pane objects so that they can be replaced by new ones.
    void retirePanes();

    /// Retires the handle objects so that they can be replaced by new ones.
    void retireSplitters();

    /// Returns the weights of the children of the given layout cell, using an even subdivision if the layout cell
    /// holds no weights (a layout that was created programmatically may have children without weights).
    static std::vector<FloatType> normalizedChildWeights(const ViewportLayoutCell* cell);

    /// Returns the effective space along the split axis of a layout cell after subtracting the handles.
    static qreal effectiveSpace(const ViewportLayoutCell* cell, const QRectF& rect, bool horizontal, qreal handleThickness);

    /// Returns the width of the gap between two panes of a layout cell.
    static qreal handleThicknessFor(const ViewportLayoutCell* cell, const QRectF& rect, bool horizontal);

    /// The user interface the layout belongs to.
    QmlMainWindowUI& _ui;

    /// Size of the area the viewports are laid out in.
    QSizeF _panelSize = QSizeF(0, 0);

    /// The panes of the viewport area, in the order of ViewportConfiguration::viewports().
    std::vector<QmlViewportPane*> _panes;

    /// The handles between the panes.
    std::vector<QmlViewportSplitter*> _splitters;

    /// Panes and handles of the previous layout, which are destroyed once the scene has dropped their delegates.
    std::vector<QmlViewportPane*> _retiredPanes;
    std::vector<QmlViewportSplitter*> _retiredSplitters;

    /// Index of the active viewport within the current viewport configuration.
    int _activeViewportIndex = -1;

    /// Whether the current layout has more than one pane and can therefore maximize a viewport.
    bool _maximizable = false;

    /// The transaction a splitter drag is recorded in. It stays open until the drag finishes.
    UndoableTransaction _dragTransaction;

    /// The layout cell and the pair of children the current drag resizes.
    OOWeakRef<ViewportLayoutCell> _dragCell;
    int _dragChildIndex = -1;

    /// Weight units per pixel along the drag axis, and where the drag started.
    qreal _dragFactor = 0;
    bool _dragHorizontal = true;
    QPointF _dragStartPosition;

    /// Width of the gap between two panes that holds the handle.
    static constexpr qreal HandleThickness = 6.0;
};

}   // End of namespace
