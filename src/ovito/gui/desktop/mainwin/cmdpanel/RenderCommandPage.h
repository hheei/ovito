// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>

namespace Ovito {

/**
 * The command panel page lets user render the scene.
 */
class OVITO_GUI_EXPORT RenderCommandPage : public QWidget
{
    Q_OBJECT

public:

    /// Initializes the command panel page.
    RenderCommandPage(MainWindowUI& userInterface, QWidget* parent);

    /// Loads the layout of the widgets from the settings store.
    void restoreLayout() {}

    /// Saves the layout of the widgets to the settings store.
    void saveLayout() {}

private Q_SLOTS:

    /// This is called when new render settings have been loaded.
    void onRenderSettingsReplaced(RenderSettings* newRenderSettings);

private:

    /// This panel shows the properties of the render settings object.
    PropertiesPanel* _propertiesPanel;
};

}   // End of namespace
