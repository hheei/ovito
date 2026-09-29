// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/undo/UndoableTransaction.h>

namespace Ovito {

/**
 * The container widget for the viewports in OVITO's main window.
 */
class OVITO_GUI_EXPORT ViewportsPanel : public QWidget
{
    Q_OBJECT

public:

    /// Constructs the viewport panel.
    explicit ViewportsPanel(MainWindow& parent);

    /// Returns the window that is associated with the given viewport (if any).
    WidgetViewportWindow* viewportWindow(Viewport* vp);

    /// Returns the widget that hosts the given viewport (if any).
    QWidget* viewportWidget(Viewport* vp);

    /// Returns the current viewport configuration object.
    ViewportConfiguration* viewportConfiguration() const { return _viewportConfig; }

    /// Handles keyboard input for the viewport windows.
    bool onKeyShortcut(QKeyEvent* event);

private Q_SLOTS:

    /// Applies the renderer the user selected to all viewport windows (see ViewportRendererRegistry).
    void viewportRendererSelectionChanged();

public Q_SLOTS:

    /// Requests a re-layout of the viewport windows.
    void invalidateWindowLayout();

    /// Performs the layout of the viewports in the panel.
    void layoutViewports();

    /// Creates the physical viewport windows for the viewports of the current viewport configuration.
    void createViewportWindows();

    /// Destroys all viewport windows in the panel and recreates them.
    void recreateViewportWindows();

    /// Handles a fatal error that occurred in a viewport window.
    void fatalViewportWindowError(Exception ex);

Q_SIGNALS:

    /// This signal is emitted when a different rendering backend for the interactive viewports has been activated.
    void interactiveViewportRendererChanged();

protected:

    /// Filters events sent to the viewport widget and window objects.
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

    /// Renders the borders around the viewports.
    virtual void paintEvent(QPaintEvent* event) override;

    /// Handles size event for the window.
    virtual void resizeEvent(QResizeEvent* event) override;

    /// Handles mouse input events.
    virtual void mousePressEvent(QMouseEvent* event) override;

    /// Handles mouse input events.
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    /// Handles mouse input events.
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    /// Handles general events of the widget.
    virtual bool event(QEvent* event) override;

private Q_SLOTS:

    /// Displays the context menu for a viewport window.
    void onViewportMenuRequested(ViewportWindow* viewportWindow, const QPoint& pos);

    /// This is called when a new viewport configuration has been loaded.
    void onViewportConfigurationReplaced(ViewportConfiguration* newViewportConfiguration);

    /// This is called when the current viewport input mode has changed.
    void onInputModeChanged(ViewportInputMode* oldMode, ViewportInputMode* newMode);

    /// This is called when the mouse cursor of the active input mode has changed.
    void onViewportModeCursorChanged(const QCursor& cursor);

private:

    struct SplitterRectangle
    {
        QRect area;
        ViewportLayoutCell* cell;
        size_t childCellIndex;
        FloatType dragFactor;
    };

    /// Recursive helper function for laying out the viewport windows.
    void layoutViewportsRecursive(ViewportLayoutCell* layoutCell, const QRect& rect);

    /// Displays the context menu associated with a splitter handle.
    void showSplitterContextMenu(const SplitterRectangle& splitter, const QPoint& mousePos);

    QMetaObject::Connection _activeModeCursorChangedConnection;

    OORef<ViewportConfiguration> _viewportConfig;
    MainWindow& _mainWindow;
    std::vector<OORef<WidgetViewportWindow>> _viewportWindows;
    bool _windowCreationErrorOccurred = false;
    bool _windowCreationIsBroken = false;
    bool _layoutRequested = false;
    std::vector<SplitterRectangle> _splitterRegions;
    int _hoveredSplitter = -1;
    bool _highlightSplitter = false;
    int _draggedSplitter = -1;
    QPoint _dragStartPos;
    QBasicTimer _highlightSplitterTimer;
    UndoableTransaction _undoTransaction;

    static constexpr int _splitterSize = 2;
    static constexpr int _windowInset = 2;
};

}   // End of namespace
