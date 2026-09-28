// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>

namespace Ovito {

/**
 * The command panel in the main window.
 */
class OVITO_GUI_EXPORT CommandPanel : public QWidget
{
    Q_OBJECT

public:

    /// Constructor.
    CommandPanel(MainWindowUI& userInterface, QWidget* parent);

    /// Activates one of the command pages.
    void setCurrentPage(MainWindow::CommandPanelPage newPage) {
        OVITO_ASSERT(newPage < _tabWidget->count());
        _tabWidget->setCurrentIndex((int)newPage);
    }

    /// Returns the currently active command page.
    MainWindow::CommandPanelPage currentPage() const { return static_cast<MainWindow::CommandPanelPage>(_tabWidget->currentIndex()); }

    /// Returns the modification page contained in the command panel.
    ModifyCommandPage* modifyPage() const { return _modifyPage; }

    /// Returns the rendering page contained in the command panel.
    RenderCommandPage* renderPage() const { return _renderPage; }

    /// Returns the viewport overlay page contained in the command panel.
    OverlayCommandPage* overlayPage() const { return _overlayPage; }

    /// Returns the utilities page contained in the command panel.
    UtilityCommandPage* utilityPage() const { return _utilityPage; }

    /// Returns the default size for the command panel.
    virtual QSize sizeHint() const { return QSize(336, 300); }

    /// Loads the layout of the widgets from the settings store.
    void restoreLayout();

    /// Saves the layout of the widgets to the settings store.
    void saveLayout();

private:

    QTabWidget* _tabWidget;
    ModifyCommandPage* _modifyPage;
    RenderCommandPage* _renderPage;
    OverlayCommandPage* _overlayPage;
    UtilityCommandPage* _utilityPage;
};

}   // End of namespace
