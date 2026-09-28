// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/viewport/QuickViewportItem.h>
#include <ovito/gui/qml/viewport/QuickViewportWindow.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
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
}

/******************************************************************************
* Returns the status message displayed in the status line.
******************************************************************************/
QString QmlViewportController::statusMessage() const
{
    return _ui.statusMessage();
}

/******************************************************************************
* Sets the status message displayed in the status line.
******************************************************************************/
void QmlViewportController::setStatusMessage(const QString& message)
{
    _ui.showStatusBarMessage(message);
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
    OVITO_ASSERT(viewportIndex >= 0);

    ViewportConfiguration* viewportConfig = _ui.datasetContainer().activeViewportConfig();
    if(!viewportConfig || viewportIndex >= viewportConfig->viewports().size())
        return nullptr;

    Viewport* viewport = viewportConfig->viewports()[viewportIndex].get();

    // Discard a viewport item that is still showing another viewport (or another dataset).
    if(auto* oldItem = viewportItem(viewportIndex)) {
        _viewportItems[viewportIndex].clear();
        oldItem->setParentItem(nullptr);
        delete oldItem;
    }

    auto* item = new QuickViewportItem(parentItem);
    item->setParentItem(parentItem);

    // Create the renderer that provides the settings used for interactive rendering.
    if(!_interactiveRenderer)
        _interactiveRenderer = OORef<StandardRenderer>::create();

    item->initializeWindow(viewport, _ui, _interactiveRenderer);

    // Keep the item sized to the area reserved for it in the QML layout.
    const auto resizeItem = [item, parentItem]() {
        item->setSize(parentItem->size());
    };
    connect(parentItem, &QQuickItem::widthChanged, this, resizeItem);
    connect(parentItem, &QQuickItem::heightChanged, this, resizeItem);
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
    for(QPointer<QuickViewportItem>& item : _viewportItems) {
        if(QuickViewportItem* viewportItem = item.data()) {
            viewportItem->setParentItem(nullptr);
            delete viewportItem;
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
