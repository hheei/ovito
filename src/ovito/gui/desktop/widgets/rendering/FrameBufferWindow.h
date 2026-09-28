// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/utilities/concurrent/FutureWatcher.h>
#include "FrameBufferWidget.h"

namespace Ovito {

/**
 * This window displays the contents of a FrameBuffer.
 */
class OVITO_GUI_EXPORT FrameBufferWindow : public QMainWindow, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    FrameBufferWindow(MainWindowUI& ui, QWidget* parent = nullptr);

    /// Return the FrameBuffer that is currently shown in the widget (can be NULL).
    const std::shared_ptr<FrameBuffer>& frameBuffer() const { return _frameBufferWidget->frameBuffer(); }

    /// Sets the FrameBuffer that is currently shown in the widget.
    void setFrameBuffer(const std::shared_ptr<FrameBuffer>& frameBuffer) { _frameBufferWidget->setFrameBuffer(frameBuffer); }

    /// Creates a frame buffer of the requested size and adjusts the size of the window.
    const std::shared_ptr<FrameBuffer>& createFrameBuffer(int w, int h);

    /// Shows and activates the frame buffer window.
    void showAndActivateWindow();

    /// Makes the framebuffer modal while a rendering operation is in progress
    /// and displays a progress indicator in the window.
    void showRenderingProgress(SharedFuture<void> renderingFuture);

public Q_SLOTS:

    /// This opens the file dialog and lets the user save the current contents of the frame buffer
    /// to an image file.
    void saveImage();

    /// This copies the current image to the clipboard.
    void copyImageToClipboard();

    /// Removes background color pixels along the outer edges of the rendered image.
    void autoCrop();

    /// Scales the image up.
    void zoomIn();

    /// Scales the image down.
    void zoomOut();

    /// Stops the rendering operation that is currently in progress.
    void cancelRendering();

protected Q_SLOTS:

    /// Is called during rendering whenever progress is made.
    void onTaskProgressUpdate();

    /// Is called when the rendering process ended (for any reason).
    void onRenderingStopped();

    /// Is called when the rendering process failed with an error.
    void onRenderingFailed(const Exception& ex);

protected:

    /// Is called when the user tries to close the window.
    virtual void closeEvent(QCloseEvent* event) override;

private:

    /// The widget that displays the FrameBuffer.
    FrameBufferWidget* _frameBufferWidget;

    // Toolbar actions:
    QAction* _saveToFileAction;
    QAction* _copyToClipboardAction;
    QAction* _autoCropAction;
    QAction* _cancelRenderingAction;

    /// Layout manager of the central container widget.
    QStackedLayout* _centralLayout;

    /// FutureWatcher for the rendering operation that is currently in progress.
    FutureWatcher<SharedFuture<void>> _renderingWatcher;

    /// Layout component for displaying the progress of rendering operations.
    QVBoxLayout* _progressLayout;

    /// List of per-task display widgets.
    std::vector<std::pair<QLabel*, QProgressBar*>> _taskWidgets;

    /// Connection to the main window's taskProgressUpdate() signal.
    QMetaObject::Connection _taskProgressUpdateConnection;
};

}   // End of namespace
