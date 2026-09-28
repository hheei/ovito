////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This dialog box lets the user configure the rendering backend and GPU adapter for interactive viewports.
 */
class ConfigureViewportGraphicsDialog : public QDockWidget, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ConfigureViewportGraphicsDialog(MainWindowUI& ui, QWidget* parentWindow);

private Q_SLOTS:

    /// Updates the values displayed in the dialog.
    void updateGUI();

    /// Is called when the user selects a different renderer for the interactive viewports.
    void backendSelectionChanged(QAbstractButton* option, bool checked);

    /// Is called when the user selects a different GPU adapter.
    void adapterSelectionChanged();

protected:

    /// Is called when the dialog window is being closed by the user.
    virtual void closeEvent(QCloseEvent* event) override;

private:

    QButtonGroup* _backendSelectionGroup; /// Group of radio buttons for selecting the rendering backend.
    QStackedWidget* _backendSettingsStack; /// Hosts the settings widgets for the different rendering backends.
    std::map<QString, int> _backendSettingsMap; /// Maps backend identifiers to the stack index of the corresponding settings widget.
    QComboBox* _gpuAdapterCombo = nullptr; /// Combo box for selecting the GPU adapter.
};

}   // End of namespace
