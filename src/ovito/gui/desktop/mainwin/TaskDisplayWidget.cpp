// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/app/TaskProgressModel.h>
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
    // Which task the status bar shows and how far it is comes from the workbench's task progress model, which the
    // Qt Quick frontend's status line reads as well; this widget only decides how to display it.
    TaskProgressModel* model = _mainWindow->ui().taskProgressModel();
    const QString activeText = model ? model->activeText() : QString();

    // Update display.
    _progressTextDisplay->setText(activeText);
    if(!activeText.isEmpty()) {
        // A task that cannot report progress (maximum zero) is displayed as an indeterminate progress bar.
        _progressBar->setRange(0, model->activeMaximum());
        _progressBar->setValue(model->activeValue());
        show();
    }
    else {
        hide();
    }
}

}   // End of namespace
