// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/OverlayListModel.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AvailableOverlaysSelectorWidget.h"
#include "ActionCardsPopup.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableOverlaysSelectorWidget::AvailableOverlaysSelectorWidget(QWidget* parent, MainWindowUI& ui, OverlayListModel* overlayListModel)
    : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui), _overlayListModel(overlayListModel)
{
    // Fill combo box with a dummy item.
    addItem(tr("Add layer..."));

    // Create the available overlays model.
    _availableOverlaysModel = new AvailableOverlaysModel(this, ui, overlayListModel);

    // Update enabled state when the active viewport changes.
    connect(&datasetContainer(), &DataSetContainer::activeViewportChanged, this, &AvailableOverlaysSelectorWidget::onActiveViewportChanged);

    // Set initial enabled state.
    onActiveViewportChanged(datasetContainer().activeViewport());
}

/******************************************************************************
* Updates the enabled state of this widget based on the current viewport.
******************************************************************************/
void AvailableOverlaysSelectorWidget::onActiveViewportChanged(Viewport* activeViewport)
{
    setEnabled(activeViewport != nullptr);
}

/******************************************************************************
* Called when the popup menu is about to be shown.
******************************************************************************/
void AvailableOverlaysSelectorWidget::showPopup()
{
    // Lazy create the card popup
    if(!_cardPopup) {
        _cardPopup = new ActionCardsPopup(availableOverlaysModel(), tr("Get more layers..."), this);
        connect(_cardPopup, &ActionCardsPopup::getMoreActionsClicked, this, &AvailableOverlaysSelectorWidget::onGetMoreLayersFromPopup);
    }
    _cardPopup->updateContent();
    _cardPopup->showBelow(this);
}

/******************************************************************************
* Handles click on "Get more layers..." button.
******************************************************************************/
void AvailableOverlaysSelectorWidget::onGetMoreLayersFromPopup()
{
    // Open the extensions gallery or website.
    if(QAction* action = actionManager()->findAction(ACTION_SCRIPTING_EXTENSIONS_GALLERY_OVERLAYS))
        action->trigger();
    else
        QDesktopServices::openUrl(QStringLiteral("https://www.ovito.org/extensions/"));
}

}   // End of namespace
