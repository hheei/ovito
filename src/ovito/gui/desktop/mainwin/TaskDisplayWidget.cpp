// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/ElidedTextLabel.h>
#include "TaskDisplayWidget.h"

namespace Ovito {

/******************************************************************************
* Constructs the widget and associates it with the main window.
******************************************************************************/
TaskDisplayWidget::TaskDisplayWidget(MainWindow* mainWindow) : _mainWindow(mainWindow)
{
    QHBoxLayout* progressWidgetLayout = new QHBoxLayout(this);
    progressWidgetLayout->setContentsMargins(10,0,0,0);
    progressWidgetLayout->setSpacing(0);
    _progressTextDisplay = new ElidedTextLabel(Qt::ElideLeft);
    _progressTextDisplay->setLineWidth(0);
    _progressTextDisplay->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    _progressTextDisplay->setAutoFillBackground(true);
    _progressTextDisplay->setMargin(2);
    _progressTextDisplay->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    progressWidgetLayout->addWidget(_progressTextDisplay);
    _progressBar = new QProgressBar(this);
    _progressBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    progressWidgetLayout->addWidget(_progressBar);
    progressWidgetLayout->addStrut(_progressTextDisplay->sizeHint().height());
    setMinimumHeight(_progressTextDisplay->minimumSizeHint().height());

    connect(mainWindow, &MainWindow::taskProgressUpdate, this, &TaskDisplayWidget::updateIndicator);
    connect(this, &QObject::destroyed, _progressTextDisplay, &QObject::deleteLater);

    updateIndicator();
}

/******************************************************************************
* Shows or hides the progress indicator widgets and updates the displayed information.
******************************************************************************/
void TaskDisplayWidget::updateIndicator()
{
    QString activeText;
    int activeValue;
    int activeMaximum;

    // Visit all in-progress tasks and pick the one that should be displayed in the status bar.
    _mainWindow->ui().visitRunningTasks([&](const QString& text, int progressValue, int progressMaximum) {
        if(!text.isEmpty() && activeText.isEmpty()) {
            activeText = text;
            activeValue = progressValue;
            activeMaximum = progressMaximum;
        }
    });

    // Update display.
    _progressTextDisplay->setText(activeText);
    if(!activeText.isEmpty()) {
        _progressBar->setRange(0, activeMaximum);
        _progressBar->setValue(activeValue);
        show();
    }
    else {
        hide();
    }
}

}   // End of namespace
