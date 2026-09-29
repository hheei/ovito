// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

class OverlayListModel; // defined in OverlayListModel.h
class OverlayListItem;  // defined in OverlayListItem.h

/**
 * The command panel tab lets the user edit the viewport overlays.
 */
class OVITO_GUI_EXPORT OverlayCommandPage : public QWidget, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Initializes the command panel page.
    OverlayCommandPage(MainWindowUI& ui, QWidget* parent);

    /// Returns the list model that encapsulates the list of overlays of the active viewport.
    OverlayListModel* overlayListModel() const { return _overlayListModel; }

    /// Loads the layout of the widgets from the settings store.
    void restoreLayout();

    /// Saves the layout of the widgets to the settings store.
    void saveLayout();

protected Q_SLOTS:

    /// This is called when another viewport became active.
    void onActiveViewportChanged(Viewport* activeViewport);

    /// Is called when a new layer has been selected in the list box.
    void onItemSelectionChanged();

    /// This deletes the selected viewport layer.
    void onDeleteLayer();

    /// This called when the user double clicks an item in the list.
    void onLayerDoubleClicked(const QModelIndex& index);

    /// Action handler moving the selected layer up in the stack.
    void onLayerMoveUp();

    /// Action handler moving the selected layer down in the stack.
    void onLayerMoveDown();

private:

    /// Returns the selected viewport layer.
    ViewportOverlay* selectedLayer() const;

    /// The Qt model for the list of overlays of the active viewport.
    OverlayListModel* _overlayListModel;

    /// This list box shows the overlays of the active viewport.
    QListView* _overlayListWidget;

    /// This panel shows the properties of the selected overlay.
    PropertiesPanel* _propertiesPanel;

    /// The GUI action that deletes the currently selected viewport layer.
    Command* _deleteLayerCommand;

    /// The GUI action that moves the currently selected viewport layer up in the stack.
    Command* _moveLayerUpCommand;

    /// The GUI action that moves the currently selected viewport layer down in the stack.
    Command* _moveLayerDownCommand;

    /// The splitter widget separating the layer list and the properties panel.
    QSplitter* _splitter;
};

}   // End of namespace
