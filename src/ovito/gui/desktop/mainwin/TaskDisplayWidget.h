// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief Displays the running tasks in the status bar of the main window.
 */
class TaskDisplayWidget : public QWidget
{
    Q_OBJECT

public:

    /// Constructs the widget and associates it with the main window.
    TaskDisplayWidget(MainWindow* mainWindow);

private Q_SLOTS:

    /// Updates the displayed information in the indicator widget.
    void updateIndicator();

private:

    /// The window this display widget is associated with.
    MainWindow* _mainWindow;

    /// The progress bar widget.
    QProgressBar* _progressBar;

    /// The label that displays the current progress text.
    QLabel* _progressTextDisplay;
};

}   // End of namespace
