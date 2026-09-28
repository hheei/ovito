// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "SystemInformationDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
SystemInformationDialog::SystemInformationDialog(UserInterface& userInterface, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("System Information"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QTextEdit* textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(userInterface.generateSystemReport());
    textEdit->setMinimumSize(QSize(600, 400));
    layout->addWidget(textEdit);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole), &QPushButton::clicked, [textEdit]() {
        QApplication::clipboard()->setText(textEdit->toPlainText());
    });
    layout->addWidget(buttonBox);
}

}   // End of namespace
