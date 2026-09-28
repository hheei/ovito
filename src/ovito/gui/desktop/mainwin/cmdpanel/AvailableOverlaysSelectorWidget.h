// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/mainwin/AvailableOverlaysModel.h>

namespace Ovito {

class OverlayListModel;  // Defined in OverlayListModel.h
class ActionCardsPopup;  // Defined in ActionCardsPopup.h

/**
 * A combo box widget that displays the list of available viewport layers and allows the user
 * to insert a layer into the current viewport.
 */
class OVITO_GUI_EXPORT AvailableOverlaysSelectorWidget : public QComboBox, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableOverlaysSelectorWidget(QWidget* parent, MainWindowUI& ui, OverlayListModel* overlayListModel);

    /// Returns the tree model that organizes all available overlays by category.
    AvailableOverlaysModel* availableOverlaysModel() const { return _availableOverlaysModel; }

protected:

    /// Called when the popup menu is about to be shown.
    virtual void showPopup() override;

private Q_SLOTS:

    /// Updates the enabled state of this widget based on the current viewport.
    void onActiveViewportChanged(Viewport* activeViewport);

    /// Handles click on "Get more layers..." button.
    void onGetMoreLayersFromPopup();

private:

    /// The model providing the available overlays.
    AvailableOverlaysModel* _availableOverlaysModel;

    /// The overlay list model used to determine the enabled state.
    OverlayListModel* _overlayListModel;

    /// The card-based popup widget (lazy initialized).
    ActionCardsPopup* _cardPopup = nullptr;
};

}   // End of namespace
