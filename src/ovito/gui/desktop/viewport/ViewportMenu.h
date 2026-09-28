// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportLayout.h>
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

/**
 * \brief The context menu of the viewports.
 */
class OVITO_GUI_EXPORT ViewportMenu : public QMenu, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Initializes the menu.
    ViewportMenu(MainWindowUI& ui, ViewportWindow* viewportWindow, QWidget* viewportWidget);

    /// Displays the menu.
    void show(const QPoint& pos);

    /// Returns the viewport this menu belongs to.
    Viewport* viewport() const { return _viewportWindow->viewport(); }

private Q_SLOTS:

    void onRenderPreviewMode(bool checked);
    void onShowGrid(bool checked);
    void onConstrainRotation(bool checked);
    void onShowViewTypeMenu();
    void onViewType(QAction* action);
    void onAdjustView();
    void onViewNode(QAction* action);
    void onCreateCamera();
    void onDeleteViewport();
    void onSplitViewport(ViewportLayoutCell::SplitDirection direction);
    void onPipelineVisibility(bool checked);

private:

    /// The viewport window this menu belongs to.
    OORef<ViewportWindow> _viewportWindow;

    /// The viewport widget this menu is shown in.
    QWidget* _viewportWidget;

    /// The view type sub-menu.
    QMenu* _viewTypeMenu;

    /// The cell in the window layout the viewport is living in.
    ViewportLayoutCell* _layoutCell;
};

}   // End of namespace
