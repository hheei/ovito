// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/viewport/QuickViewportItem.h>
#include <ovito/gui/qml/viewport/QuickViewportWindow.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/gui/base/viewport/ViewportRendererRegistry.h>
#include "QmlViewportController.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QmlViewportController::QmlViewportController(QmlMainWindowUI& ui, QObject* parent) : QObject(parent), _ui(ui)
{
    // The viewport items have to be rebuilt whenever the current dataset (and with it the set of viewports) changes,
    // because a viewport item belongs to the viewport of one dataset.
    connect(&ui.datasetContainer(), &DataSetContainer::dataSetChanged, this, [this]() {
        discardViewportItems();
        Q_EMIT viewportConfigurationChanged();
    });

    // When the user selects another renderer for the interactive viewports, the viewport items have to be given the new
    // renderer instance (see ViewportRendererRegistry).
    connect(&ViewportRendererRegistry::instance(), &ViewportRendererRegistry::rendererSelectionChanged, this, [this]() {
        applyInteractiveRenderer();
    });
}

/******************************************************************************
* Gives every viewport item the renderer the user selected for the interactive viewports.
******************************************************************************/
void QmlViewportController::applyInteractiveRenderer()
{
    // The instance stays alive in the registry, so the viewport windows can take a plain pointer to it.
    const OORef<SceneRenderer> renderer = interactiveRenderer();
    if(!renderer)
        return;
    _interactiveRenderer = renderer;
    for(const QPointer<QuickViewportItem>& item : _viewportItems) {
        if(item && item->viewportWindow())
            item->viewportWindow()->setSceneRenderer(renderer);
    }
}

/******************************************************************************
* Returns the renderer that provides the settings of the interactive viewports.
******************************************************************************/
OORef<SceneRenderer> QmlViewportController::interactiveRenderer()
{
    // The registry falls back to the default renderer if the one the user selected is not available in this build.
    return ViewportRendererRegistry::instance().renderer();
}

/******************************************************************************
* Handles a fatal error of a viewport window.
******************************************************************************/
void QmlViewportController::viewportWindowFatalError(const Exception& ex)
{
    if(_fatalRenderErrorHandled) // An error has already been handled; do not report another one or retry in a loop.
        return;
    _fatalRenderErrorHandled = true;

    if(!ViewportRendererRegistry::instance().revertToDefaultRenderer()) {
        // The default renderer is already in use, so there is nothing to fall back to. Retrying would only produce the
        // same error again.
        Exception error = ex;
        error.prependGeneralMessage(tr("There is a critical problem with the interactive viewport windows."));
        _ui.reportError(error, true);
        return;
    }
    // The registry announces the change, which gives every viewport window the default renderer again.
    _ui.reportError(ex, false);
}

/******************************************************************************
* Returns the number of viewports of the current dataset.
******************************************************************************/
int QmlViewportController::viewportCount() const
{
    if(ViewportConfiguration* viewportConfig = _ui.datasetContainer().activeViewportConfig())
        return viewportConfig->viewports().size();
    return 0;
}

/******************************************************************************
* Creates the viewport item for the viewport with the given index of the current dataset.
******************************************************************************/
QQuickItem* QmlViewportController::createViewportItem(QQuickItem* parentItem, int viewportIndex)
{
    OVITO_ASSERT(parentItem);

    // QML calls this while it instantiates the workbench, which is a callback of the presentation layer and therefore
    // has no task context of its own; creating the renderer and the viewport item needs one.
    GuiTaskScope taskScope(_ui);

    ViewportConfiguration* viewportConfig = _ui.datasetContainer().activeViewportConfig();
    // The index comes from the QML scene, so it is validated instead of trusted: an index that does not belong to the
    // current set of viewports would otherwise be used to index into the list of viewport items.
    if(!parentItem || !viewportConfig || viewportIndex < 0 || viewportIndex >= (int)viewportConfig->viewports().size())
        return nullptr;

    Viewport* viewport = viewportConfig->viewports()[viewportIndex].get();

    // Replace the viewport item that is still showing another viewport, or that belongs to a pane delegate the scene
    // has rebuilt. The item cannot simply be destroyed here: QML calls this while it creates the workbench or while it
    // processes an event that may be delivered to the item being replaced, and destroying an object underneath its own
    // event handling crashes the process. Retiring it with deleteLater() also keeps it alive long enough for the pane
    // delegate that used to own it to be gone.
    if(QuickViewportItem* existingItem = viewportItem(viewportIndex)) {
        _viewportItems[viewportIndex].clear();
        existingItem->setParentItem(nullptr);
        existingItem->deleteLater();
    }

    auto* item = new QuickViewportItem(parentItem);
    item->setParentItem(parentItem);

    // Create the renderer that provides the settings used for interactive rendering. The instance is shared by all
    // viewport windows of the application, so a setting the user changes in the renderer's property editor applies to
    // every viewport.
    if(!_interactiveRenderer)
        _interactiveRenderer = interactiveRenderer();

    item->initializeWindow(viewport, _ui, _interactiveRenderer);

    // A viewport window that cannot render reports it, and the workbench falls back to the default renderer instead of
    // leaving the user with a broken viewport (the classic frontend does the same).
    if(QuickViewportWindow* viewportWindow = item->viewportWindow())
        connect(viewportWindow, &ViewportWindow::fatalError, this, &QmlViewportController::viewportWindowFatalError);

    // Keep the item sized to the area reserved for it in the QML layout. The item is the context of the connections, so
    // that they die with it instead of leaving a dangling reference behind in a later resize of the pane.
    const auto resizeItem = [item, parentItem]() {
        item->setSize(parentItem->size());
    };
    connect(parentItem, &QQuickItem::widthChanged, item, resizeItem);
    connect(parentItem, &QQuickItem::heightChanged, item, resizeItem);
    resizeItem();

    if(_viewportItems.size() <= viewportIndex)
        _viewportItems.resize(viewportIndex + 1);
    _viewportItems[viewportIndex] = item;
    return item;
}

/******************************************************************************
* Returns the viewport item that has been created for the viewport with the given index.
******************************************************************************/
QuickViewportItem* QmlViewportController::viewportItem(int viewportIndex) const
{
    if(viewportIndex >= 0 && viewportIndex < _viewportItems.size())
        return _viewportItems[viewportIndex].data();
    return nullptr;
}

/******************************************************************************
* Discards the viewport items of the previous dataset.
******************************************************************************/
void QmlViewportController::discardViewportItems()
{
    // The items are retired with deleteLater(): they are destroyed together with the dataset of the viewports they
    // show, which may happen while an event is being delivered to one of them, and destroying such an item right away
    // crashes the process.
    for(QPointer<QuickViewportItem>& item : _viewportItems) {
        if(QuickViewportItem* viewportItem = item.data()) {
            viewportItem->setParentItem(nullptr);
            viewportItem->deleteLater();
        }
    }
    _viewportItems.clear();
}

/******************************************************************************
* Gives the input focus to the viewport item displaying the given viewport.
******************************************************************************/
void QmlViewportController::setViewportInputFocus(Viewport* viewport)
{
    if(!viewport)
        return;
    for(const QPointer<QuickViewportItem>& item : _viewportItems) {
        if(item && item->viewportWindow() && item->viewportWindow()->viewport() == viewport) {
            item->forceActiveFocus();
            break;
        }
    }
}

}   // End of namespace
