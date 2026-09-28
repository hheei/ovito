// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

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
