// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/OverlayCommandPage.h>
#include <ovito/gui/base/mainwin/OverlayListModel.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "OverlayTemplatesPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OverlayTemplatesPage);

/******************************************************************************
* When the user is creating a new template, this method populates the list of available objects,
* which the user can select to be included in the template.
******************************************************************************/
QVector<QTreeWidgetItem*> OverlayTemplatesPage::populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox)
{
    OverlayListModel* overlayModel = ui().mainWindow()->commandPanel()->overlayPage()->overlayListModel();
    ViewportOverlay* selectedOverlay = overlayModel->selectedLayer();
    QVector<QTreeWidgetItem*> itemList;

    // Iterate over the overlays of the selected viewport.
    if(Viewport* viewport = overlayModel->selectedViewport()) {
        QVector<OORef<ViewportOverlay>> layers;
        layers.append(viewport->underlays());
        layers.append(viewport->overlays());
        for(auto layer = layers.crbegin(); layer != layers.crend(); ++layer) {
            QTreeWidgetItem* listItem = new QTreeWidgetItem(objectListWidget, { (*layer)->objectTitle() });
            listItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
                listItem->setCheckState(0, (*layer == selectedOverlay) ? Qt::Checked : Qt::Unchecked);
            listItem->setData(0, Qt::UserRole, QVariant::fromValue(OORef<OvitoObject>(*layer)));
            itemList.push_back(listItem);
        }
    }
    if(itemList.empty())
        throw Exception(tr("A viewport layer template must always be created on the basis of an existing layer, but the selected viewport does not have any layers attached. "
                            "Please close this dialog, add a layer to the viewport, configure its settings, and then return here to create a template from it."));
    objectListWidget->setMaximumHeight(objectListWidget->sizeHintForRow(0) * qBound(3, itemList.size(), 10) + 2 * objectListWidget->frameWidth());

    if(selectedOverlay) {
        if(selectedOverlay->title().isEmpty())
            nameBox->setCurrentText(tr("Custom %1").arg(selectedOverlay->objectTitle()));
        else
            nameBox->setCurrentText(selectedOverlay->title());
    }
    else {
        nameBox->setCurrentText(tr("Custom viewport layer template 1"));
    }

    return itemList;
}

}   // End of namespace
