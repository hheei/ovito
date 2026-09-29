// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "QmlViewportLayout.h"

#include <algorithm>
#include <numeric>

namespace Ovito {

/******************************************************************************
* Constructor of the pane object.
******************************************************************************/
QmlViewportPane::QmlViewportPane(int viewportIndex, OORef<Viewport> viewport, QObject* parent) :
    QObject(parent), _viewportIndex(viewportIndex), _viewport(std::move(viewport))
{
    OVITO_ASSERT(_viewport);
}

/******************************************************************************
* Sets the geometry of the pane.
******************************************************************************/
void QmlViewportPane::setGeometry(const QRectF& rect)
{
    if(_rect == rect)
        return;
    _rect = rect;
    Q_EMIT rectChanged();
}

/******************************************************************************
* Sets the state of the pane.
******************************************************************************/
void QmlViewportPane::setState(int viewportIndex, bool active, bool maximized, bool visible, bool maximizable)
{
    if(_viewportIndex != viewportIndex) {
        _viewportIndex = viewportIndex;
        Q_EMIT viewportIndexChanged();
    }
    if(_active != active) {
        _active = active;
        Q_EMIT activeChanged();
    }
    if(_maximized != maximized) {
        _maximized = maximized;
        Q_EMIT maximizedChanged();
    }
    if(_visible != visible) {
        _visible = visible;
        Q_EMIT visibleChanged();
    }
    if(_maximizable != maximizable) {
        _maximizable = maximizable;
        Q_EMIT maximizableChanged();
    }
}

/******************************************************************************
* Constructor of the splitter handle object.
******************************************************************************/
QmlViewportSplitter::QmlViewportSplitter(QObject* parent) : QObject(parent) {}

/******************************************************************************
* Sets the geometry of the handle.
******************************************************************************/
void QmlViewportSplitter::setGeometry(const QRectF& rect, bool horizontal)
{
    if(_rect != rect) {
        _rect = rect;
        Q_EMIT rectChanged();
    }
    if(_horizontal != horizontal) {
        _horizontal = horizontal;
        Q_EMIT orientationChanged();
    }
}

/******************************************************************************
* Sets which layout cell and which pair of children this handle resizes.
******************************************************************************/
void QmlViewportSplitter::setLayoutCell(ViewportLayoutCell* cell, int childIndex)
{
    OVITO_ASSERT(cell && childIndex >= 0);
    _cell = cell;
    _childIndex = childIndex;
}

/******************************************************************************
* Passes the data a drag of this handle needs.
******************************************************************************/
void QmlViewportSplitter::setDragParameters(const QRectF& cellRect, qreal dragFactor)
{
    _cellRect = cellRect;
    _dragFactor = dragFactor;
}

/******************************************************************************
* Constructor.
******************************************************************************/
QmlViewportLayout::QmlViewportLayout(QmlMainWindowUI& ui, QObject* parent) : QObject(parent), _ui(ui)
{
    // The layout model mirrors the layout of the current dataset, so it has to follow all changes to that layout -
    // including the ones made by the undo system when the user undoes a splitter drag.
    DataSetContainer& datasetContainer = _ui.datasetContainer();
    connect(&datasetContainer, &DataSetContainer::dataSetChanged, this, &QmlViewportLayout::updateLayout);
    connect(&datasetContainer, &DataSetContainer::viewportLayoutChanged, this, &QmlViewportLayout::updateLayout);
    connect(&datasetContainer, &DataSetContainer::activeViewportChanged, this, &QmlViewportLayout::updateLayout);
    connect(&datasetContainer, &DataSetContainer::maximizedViewportChanged, this, &QmlViewportLayout::updateLayout);
    updateLayout();
}

/******************************************************************************
* Returns the panes of the viewport area.
******************************************************************************/
QVariantList QmlViewportLayout::panes() const
{
    // The list is deliberately only reported to QML when the *structure* of the layout changes: the geometry of an
    // existing pane is propagated through the pane object itself, so that QML keeps the delegate - and with it the
    // viewport item that holds the GPU resources - while the user drags a splitter or resizes the window.
    QVariantList list;
    list.reserve(_panes.size());
    for(QmlViewportPane* pane : _panes)
        list.push_back(QVariant::fromValue<QObject*>(pane));
    return list;
}

/******************************************************************************
* Returns the handles between the panes of the viewport area.
******************************************************************************/
QVariantList QmlViewportLayout::splitters() const
{
    QVariantList list;
    list.reserve(_splitters.size());
    for(QmlViewportSplitter* splitter : _splitters)
        list.push_back(QVariant::fromValue<QObject*>(splitter));
    return list;
}

/******************************************************************************
* Informs the model about the size of the area the viewports are laid out in.
******************************************************************************/
void QmlViewportLayout::setPanelSize(qreal width, qreal height)
{
    const QSizeF size(std::max(width, qreal(0)), std::max(height, qreal(0)));
    if(_panelSize == size)
        return;
    _panelSize = size;
    updateLayout();
}

/******************************************************************************
* Lays the given layout cell out in the given rectangle and collects the pane and handle geometries.
******************************************************************************/
void QmlViewportLayout::collectLayout(ViewportLayoutCell* cell, const QRectF& rect, std::vector<PaneGeometry>& panes, std::vector<SplitterGeometry>& splitters) const
{
    OVITO_ASSERT(cell);

    // A cell may hold a viewport and children at the same time; the classic frontend lays it out the same way, and the
    // order of the panes must match the order of ViewportConfiguration::viewports() for the viewport indices to be right.
    if(Viewport* viewport = cell->viewport())
        panes.push_back({viewport, rect, true});

    if(cell->children().empty())
        return;

    const bool horizontal = (cell->splitDirection() == ViewportLayoutCell::Horizontal);
    const std::vector<FloatType> weights = normalizedChildWeights(cell);
    qreal totalWeights = std::accumulate(weights.begin(), weights.end(), qreal(0));
    OVITO_ASSERT(totalWeights > 0);

    const qreal handleThickness = handleThicknessFor(cell, rect, horizontal);
    const qreal available = effectiveSpace(cell, rect, horizontal, handleThickness);

    // The number of weight units that corresponds to one pixel of pointer movement while dragging one of the handles.
    const qreal dragFactor = (available > 0) ? totalWeights / available : qreal(0);

    const qreal origin = horizontal ? rect.left() : rect.top();
    qreal offset = 0;
    for(qsizetype index = 0; index < (qsizetype)cell->children().size(); index++) {
        const qreal start = origin + index * handleThickness + (offset / totalWeights) * available;
        const qreal size = (weights[index] / totalWeights) * available;

        QRectF childRect = rect;
        if(horizontal) {
            childRect.setLeft(start);
            childRect.setWidth(size);
        }
        else {
            childRect.setTop(start);
            childRect.setHeight(size);
        }
        collectLayout(cell->children()[index], childRect, panes, splitters);

        // The handle between this child and the next one occupies the gap in between.
        if(index + 1 < (qsizetype)cell->children().size()) {
            QRectF handleRect = rect;
            if(horizontal) {
                handleRect.setLeft(start + size);
                handleRect.setWidth(handleThickness);
            }
            else {
                handleRect.setTop(start + size);
                handleRect.setHeight(handleThickness);
            }
            splitters.push_back({cell, (int)index, horizontal, handleRect, rect, dragFactor});        }
        offset += weights[index];
    }
}

/******************************************************************************
* Returns the weights of the children of a layout cell.
******************************************************************************/
std::vector<FloatType> QmlViewportLayout::normalizedChildWeights(const ViewportLayoutCell* cell)
{
    std::vector<FloatType> weights = cell->childWeights();
    if(weights.size() != cell->children().size() || std::accumulate(weights.begin(), weights.end(), FloatType(0)) <= 0)
        weights.assign(cell->children().size(), FloatType(1));
    return weights;
}

/******************************************************************************
* Returns the width of the gap between two panes.
******************************************************************************/
qreal QmlViewportLayout::handleThicknessFor(const ViewportLayoutCell* cell, const QRectF& rect, bool horizontal)
{
    const qreal extent = horizontal ? rect.width() : rect.height();
    const qreal gaps = HandleThickness * (cell->children().size() - 1);
    // A panel that has become too small to hold the handles is laid out without any gap rather than with pane
    // rectangles of negative size.
    return (extent - gaps >= cell->children().size()) ? HandleThickness : qreal(0);
}

/******************************************************************************
* Returns the effective space along the split axis of a layout cell after subtracting the handles.
******************************************************************************/
qreal QmlViewportLayout::effectiveSpace(const ViewportLayoutCell* cell, const QRectF& rect, bool horizontal, qreal handleThickness)
{
    const qreal extent = (horizontal ? rect.width() : rect.height()) - handleThickness * (cell->children().size() - 1);
    return std::max(extent, qreal(0));
}

/******************************************************************************
* Rebuilds the panes and handles from the viewport configuration of the current dataset.
******************************************************************************/
void QmlViewportLayout::updateLayout()
{
    ViewportConfiguration* config = _ui.datasetContainer().activeViewportConfig();

    std::vector<PaneGeometry> paneGeometries;
    std::vector<SplitterGeometry> splitterGeometries;
    if(config) {
        // The panes are created even before the QML scene has reported the size of the viewport area. That makes the QML
        // delegates - and with them the viewport items - part of loading the workbench window, i.e. they exist before a
        // data set is imported. This is what the classic frontend does as well (its ViewportsPanel creates the viewport
        // windows when the data set appears), and it matters: the importer asks the viewports to zoom to the scene
        // extents, which only the existing viewport windows can do. The size arrives right afterwards and merely updates
        // the pane rectangles.
        const QRectF panelRect(QPointF(0, 0), _panelSize);
        if(Viewport* maximizedViewport = config->maximizedViewport()) {
            // A maximized viewport fills the whole viewport area. The other panes stay in the model but are hidden, so
            // that the viewport items survive the transition and release their GPU resources while they are hidden.
            for(Viewport* viewport : config->viewports())
                paneGeometries.push_back({viewport, panelRect, viewport == maximizedViewport});
        }
        else if(ViewportLayoutCell* rootCell = config->layoutRootCell()) {
            collectLayout(rootCell, panelRect, paneGeometries, splitterGeometries);
        }
    }

    // Structural changes recreate the objects, everything else updates them in place. A pane object must not be
    // replaced while the user drags a splitter: its QML delegate owns the viewport item, and rebuilding the item would
    // destroy and recreate the GPU resources of the viewport on every mouse move.
    const bool panesStructureChanged = paneGeometries.size() != _panes.size() ||
        !std::equal(paneGeometries.begin(), paneGeometries.end(), _panes.begin(),
            [](const PaneGeometry& geometry, const QmlViewportPane* pane) { return geometry.viewport == pane->viewport(); });
    const bool splittersStructureChanged = splitterGeometries.size() != _splitters.size() ||
        !std::equal(splitterGeometries.begin(), splitterGeometries.end(), _splitters.begin(),
            [](const SplitterGeometry& geometry, const QmlViewportSplitter* splitter) {
                return geometry.cell == splitter->cell() && geometry.childIndex == splitter->childIndex();
            });

    // The objects of a kind of structure that changed are retired: the QML delegates of the previous list still
    // hold references to them and the scene processes the model change only after this update returns. Deleting
    // the objects here would leave those delegates with dangling pointers, which shows up as "Cannot read property
    // ... of null" errors and, once the scene uses such an object, as a crash. They are therefore retired for one
    // update and destroyed afterwards, when the delegates that displayed them are certainly gone.
    //
    // Note that only the kind of object that is replaced can be discarded: the panes of a maximized layout, for
    // instance, are the same as before while the handles are gone, and discarding the panes as well would leave
    // the list empty although the geometries are still there - the delegates would lose their viewport items.
    if(panesStructureChanged)
        retirePanes();
    if(splittersStructureChanged)
        retireSplitters();

    // The number of viewports in the layout and their order define the viewport indices the QML scene passes to
    // QmlViewportController::createViewportItem().
    std::vector<Viewport*> viewportList;
    if(config) {
        viewportList.reserve(config->viewports().size());
        for(Viewport* viewport : config->viewports())
            viewportList.push_back(viewport);
    }
    const auto viewportIndex = [&viewportList](const Viewport* viewport) {
        const auto iter = std::find(viewportList.begin(), viewportList.end(), viewport);
        return iter == viewportList.end() ? -1 : (int)std::distance(viewportList.begin(), iter);
    };

    Viewport* activeViewport = config ? config->activeViewport() : nullptr;
    Viewport* maximizedViewport = config ? config->maximizedViewport() : nullptr;
    const bool maximizable = config && config->layoutRootCell() && !config->layoutRootCell()->children().empty();

    if(panesStructureChanged) {
        _panes.reserve(paneGeometries.size());
        for(const PaneGeometry& geometry : paneGeometries)
            _panes.push_back(new QmlViewportPane(viewportIndex(geometry.viewport), geometry.viewport, this));
    }
    OVITO_ASSERT(_panes.size() == paneGeometries.size());
    for(size_t index = 0; index < _panes.size(); index++) {
        const PaneGeometry& geometry = paneGeometries[index];
        _panes[index]->setGeometry(geometry.rect);
        _panes[index]->setState(viewportIndex(geometry.viewport), geometry.viewport == activeViewport,
            geometry.viewport == maximizedViewport, geometry.visible, maximizable);
    }

    if(splittersStructureChanged) {
        _splitters.reserve(splitterGeometries.size());
        for(const SplitterGeometry& geometry : splitterGeometries) {
            auto* splitter = new QmlViewportSplitter(this);
            splitter->setLayoutCell(geometry.cell, geometry.childIndex);
            _splitters.push_back(splitter);
        }
    }
    OVITO_ASSERT(_splitters.size() == splitterGeometries.size());
    for(size_t index = 0; index < _splitters.size(); index++) {
        const SplitterGeometry& geometry = splitterGeometries[index];
        _splitters[index]->setGeometry(geometry.rect, geometry.horizontal);
        _splitters[index]->setDragParameters(geometry.cellRect, geometry.dragFactor);
    }

    if(panesStructureChanged)
        Q_EMIT panesChanged();
    if(splittersStructureChanged)
        Q_EMIT splittersChanged();

    const int activeIndex = activeViewport ? viewportIndex(activeViewport) : -1;
    if(_activeViewportIndex != activeIndex) {
        _activeViewportIndex = activeIndex;
        Q_EMIT activeViewportIndexChanged();
    }
    if(_maximizable != maximizable) {
        _maximizable = maximizable;
        Q_EMIT maximizableChanged();
    }
}

/******************************************************************************
* Retires the pane objects so that they can be replaced.
******************************************************************************/
void QmlViewportLayout::retirePanes()
{
    for(QmlViewportPane* pane : _retiredPanes)
        pane->deleteLater();
    _retiredPanes = std::move(_panes);
    _panes.clear();
}

/******************************************************************************
* Retires the handle objects so that they can be replaced.
******************************************************************************/
void QmlViewportLayout::retireSplitters()
{
    for(QmlViewportSplitter* splitter : _retiredSplitters)
        splitter->deleteLater();
    _retiredSplitters = std::move(_splitters);
    _splitters.clear();
}

/******************************************************************************
* Starts dragging the handle with the given index.
******************************************************************************/
bool QmlViewportLayout::beginSplitterDrag(int splitterIndex, qreal mouseX, qreal mouseY)
{
    // The drag is started by a mouse handler of the QML scene, i.e. outside of any OVITO task, while resizing the panes
    // goes through the undo system, which expects a task context.
    GuiTaskScope taskScope(_ui);
    if(_dragTransaction.operation()) {
        OVITO_ASSERT(false);    // The Qt Quick mouse handling should never start two drags at the same time.
        return false;
    }
    if(splitterIndex < 0 || splitterIndex >= (int)_splitters.size())
        return false;

    QmlViewportSplitter* splitter = _splitters[splitterIndex];
    ViewportLayoutCell* cell = splitter->cell();
    if(!cell || splitter->childIndex() + 1 >= (int)cell->children().size())
        return false;

    // Remember what this drag resizes, so that the drag survives a rebuild of the handle objects.
    _dragCell = cell;
    _dragChildIndex = splitter->childIndex();
    _dragHorizontal = splitter->isHorizontal();
    _dragStartPosition = QPointF(mouseX, mouseY);
    _dragFactor = splitter->dragFactor();

    // The whole drag is one undoable operation, which means it can be undone with one Undo command.
    _dragTransaction.begin(_ui, tr("Resize viewports"));
    return true;
}

/******************************************************************************
* Moves the handle that is currently being dragged to the given pointer position.
******************************************************************************/
void QmlViewportLayout::dragSplitter(qreal mouseX, qreal mouseY)
{
    GuiTaskScope taskScope(_ui);
    if(!_dragTransaction.operation())
        return;

    ViewportLayoutCell* cell = _dragCell.lock().get();
    if(!cell || _dragChildIndex < 0 || _dragChildIndex + 1 >= (int)cell->children().size()) {
        // The layout changed while the user was dragging the handle; there is nothing sensible left to resize.
        cancelSplitterDrag();
        return;
    }

    // Undo the previous application of this drag and apply the total offset instead. This keeps the recorded undo step
    // free of the intermediate positions the pointer passed through, and it keeps the minimum pane size enforced
    // relative to the pane sizes the drag started from.
    _dragTransaction.revert();

    _ui.performActions(_dragTransaction, [&]() {
        std::vector<FloatType> weights = normalizedChildWeights(cell);
        const qreal weightPerPixel = _dragFactor;
        FloatType delta = (_dragHorizontal ? mouseX - _dragStartPosition.x() : mouseY - _dragStartPosition.y()) * weightPerPixel;

        // No pane may become smaller than a tenth of the space the layout cell occupies.
        const FloatType minWeight = FloatType(0.1) * std::accumulate(weights.begin(), weights.end(), FloatType(0));
        const FloatType lowerBound = minWeight - weights[_dragChildIndex];
        const FloatType upperBound = weights[_dragChildIndex + 1] - minWeight;
        if(lowerBound > upperBound)
            return;     // There is no room to move the handle at all.
        delta = std::clamp(delta, lowerBound, upperBound);

        weights[_dragChildIndex] += delta;
        weights[_dragChildIndex + 1] -= delta;
        cell->setChildWeights(std::move(weights));
    });
}

/******************************************************************************
* Finishes the drag of the current handle.
******************************************************************************/
void QmlViewportLayout::endSplitterDrag()
{
    GuiTaskScope taskScope(_ui);
    if(!_dragTransaction.operation())
        return;
    _dragTransaction.commit();
    _dragCell.reset();
    _dragChildIndex = -1;
}

/******************************************************************************
* Aborts the drag of the current handle.
******************************************************************************/
void QmlViewportLayout::cancelSplitterDrag()
{
    GuiTaskScope taskScope(_ui);
    if(!_dragTransaction.operation())
        return;
    _dragTransaction.cancel();
    _dragCell.reset();
    _dragChildIndex = -1;
}

/******************************************************************************
* Restores the pane sizes on both sides of the handle.
******************************************************************************/
void QmlViewportLayout::resetSplitter(int splitterIndex)
{
    GuiTaskScope taskScope(_ui);
    if(splitterIndex < 0 || splitterIndex >= (int)_splitters.size())
        return;
    ViewportLayoutCell* cell = _splitters[splitterIndex]->cell();
    if(!cell)
        return;
    _ui.performTransaction(tr("Resize viewports"), [&]() {
        cell->setChildWeights(std::vector<FloatType>(cell->children().size(), FloatType(1)));
    });
}

/******************************************************************************
* Maximizes the given viewport, or restores the former layout.
******************************************************************************/
void QmlViewportLayout::toggleMaximize(int viewportIndex)
{
    GuiTaskScope taskScope(_ui);
    ViewportConfiguration* config = _ui.datasetContainer().activeViewportConfig();
    if(!config || !config->layoutRootCell() || config->layoutRootCell()->children().empty())
        return;

    Viewport* viewport = (viewportIndex >= 0 && viewportIndex < (int)config->viewports().size()) ? config->viewports()[viewportIndex].get() : nullptr;
    if(!viewport)
        return;

    _ui.handleExceptions([&]() {
        // Restore the pane that is currently maximized if the user asked for a different one. Maximizing itself is the
        // job of the shared command, which also remembers the maximized viewport across program sessions.
        if(config->maximizedViewport() && config->maximizedViewport() != viewport)
            _ui.actionManager()->triggerCommand(ACTION_VIEWPORT_MAXIMIZE);

        // The command acts on the active viewport, so the pane the user clicked becomes the active one.
        if(config->maximizedViewport() != viewport)
            config->setActiveViewport(viewport);

        _ui.actionManager()->triggerCommand(ACTION_VIEWPORT_MAXIMIZE);
    });
}

}   // End of namespace
