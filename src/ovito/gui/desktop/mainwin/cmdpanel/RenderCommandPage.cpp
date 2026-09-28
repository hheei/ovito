// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/rendering/RenderSettings.h>
#include "RenderCommandPage.h"

namespace Ovito {

/******************************************************************************
* Initializes the command panel page.
******************************************************************************/
RenderCommandPage::RenderCommandPage(MainWindowUI& userInterface, QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2,2,2,2);

    // Create the properties panel.
    _propertiesPanel = new PropertiesPanel(userInterface);
    _propertiesPanel->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
    layout->addWidget(_propertiesPanel, 1);

    connect(&userInterface.datasetContainer(), &DataSetContainer::renderSettingsReplaced, this, &RenderCommandPage::onRenderSettingsReplaced);
}

/******************************************************************************
* This is called when new render settings have been loaded.
******************************************************************************/
void RenderCommandPage::onRenderSettingsReplaced(RenderSettings* newRenderSettings)
{
    _propertiesPanel->setEditObject(newRenderSettings);
}

}   // End of namespace
