// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/viewport/QuickViewportItem.h>
#include <ovito/gui/qml/viewport/QuickViewportWindow.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
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
* Creates a viewport item for the next viewport of the current dataset.
******************************************************************************/
QQuickItem* QmlViewportController::createViewportItem(QQuickItem* parentItem)
{
    OVITO_ASSERT(parentItem);

    DataSet* dataset = _ui.datasetContainer().currentSet();
    if(!dataset || !parentItem)
        return nullptr;

    // Obtain the list of viewports of the current dataset.
    ViewportConfiguration* viewportConfig = _ui.datasetContainer().activeViewportConfig();
    if(!viewportConfig)
        return nullptr;
    const QList<OORef<Viewport>>& viewports = viewportConfig->viewports();
    if(viewports.empty())
        return nullptr;

    // The prototype creates one viewport item per call, cycling through the viewports of the dataset.
    Viewport* viewport = viewports[_nextViewportIndex % viewports.size()].get();
    _nextViewportIndex++;

    auto* item = new QuickViewportItem(parentItem);
    item->setParentItem(parentItem);

    // Create the renderer that provides the settings used for interactive rendering.
    if(!_interactiveRenderer)
        _interactiveRenderer = OORef<StandardRenderer>::create();

    item->initializeWindow(viewport, _ui, _interactiveRenderer);

    // Keep the item sized to the area reserved for it in the QML layout.
    auto resizeItem = [item, parentItem]() {
        item->setSize(parentItem->size());
    };
    connect(parentItem, &QQuickItem::widthChanged, this, resizeItem);
    connect(parentItem, &QQuickItem::heightChanged, this, resizeItem);
    resizeItem();

    _viewportItems.push_back(item);
    return item;
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
