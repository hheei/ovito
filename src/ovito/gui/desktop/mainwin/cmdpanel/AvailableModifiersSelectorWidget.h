// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/mainwin/AvailableModifiersModel.h>

namespace Ovito {

class PipelineListModel;      // Defined in PipelineListModel.h
class ActionCardsPopup;       // Defined in ActionCardsPopup.h

/**
 * A combo box widget that displays the list of available modifiers and allows the user
 * to insert a modifier into the current data pipeline.
 */
class OVITO_GUI_EXPORT AvailableModifiersSelectorWidget : public QComboBox, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableModifiersSelectorWidget(QWidget* parent, MainWindowUI& ui, PipelineListModel* pipelineListModel);

    /// Returns the tree model that organizes all available modifiers by category.
    AvailableModifiersModel* availableModifiersModel() const { return _availableModifiersModel; }

protected:

    /// Called when the popup menu is about to be shown.
    virtual void showPopup() override;

private Q_SLOTS:

    /// Updates the enabled state of this widget based on the current pipeline selection.
    void onPipelineSelectionChanged();

    /// Handles click on "Get more modifiers..." button.
    void onGetMoreModifiersFromPopup();

private:

    /// The model providing the available modifiers.
    AvailableModifiersModel* _availableModifiersModel;

    /// The pipeline list model used to determine the enabled state.
    PipelineListModel* _pipelineListModel;

    /// The card-based popup widget (lazy initialized).
    ActionCardsPopup* _cardPopup = nullptr;
};

}   // End of namespace
