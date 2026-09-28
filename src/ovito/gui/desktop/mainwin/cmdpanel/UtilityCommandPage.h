// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>

namespace Ovito {

class UtilityListModel; // defined in UtilityListModel.h

/**
 * The command panel tab lets the user access utility functions.
 */
class OVITO_GUI_EXPORT UtilityCommandPage : public QWidget, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Initializes the command panel page.
    UtilityCommandPage(MainWindowUI& ui, QWidget* parent);

protected Q_SLOTS:

    /// Called when the user selected a utility from the drop-down list of available utilities.
    void onOpenUtility(int index);

private:

    /// Returns the selected viewport layer.
    ViewportOverlay* selectedLayer() const;

    /// Contains the list of available utilities.
    QComboBox* _utilitiesBox;

    /// This panel shows the GUI of the selected utility.
    PropertiesPanel* _propertiesPanel;

    /// The list model containing the available utilities.
    UtilityListModel* _utilityListModel;
};

}   // End of namespace
