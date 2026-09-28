// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "RemoteAuthenticationDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
RemoteAuthenticationDialog::RemoteAuthenticationDialog(QWidget* parent, const QString& title, const QString& labelText) : QDialog(parent)
{
    setWindowTitle(title);

    QVBoxLayout* layout1 = new QVBoxLayout(this);
    layout1->setSpacing(2);

    QLabel* label = new QLabel(labelText);
    //label->setWordWrap(true);
    layout1->addWidget(label);
    layout1->addSpacing(10);

    layout1->addWidget(new QLabel(tr("Login:")));
    _usernameEdit = new QLineEdit(this);
    layout1->addWidget(_usernameEdit);
    layout1->addSpacing(10);

    layout1->addWidget(new QLabel(tr("Password:")));
    _passwordEdit = new QLineEdit(this);
    _passwordEdit->setEchoMode(QLineEdit::Password);
    layout1->addWidget(_passwordEdit);
    layout1->addSpacing(10);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RemoteAuthenticationDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RemoteAuthenticationDialog::reject);
    layout1->addWidget(buttonBox);
}

/******************************************************************************
* Displays the dialog.
******************************************************************************/
int RemoteAuthenticationDialog::exec()
{
    if(_usernameEdit->text().isEmpty()) {

        if(qEnvironmentVariableIsSet("USER"))
            _usernameEdit->setText(QString::fromLocal8Bit(qgetenv("USER")));
        else if(qEnvironmentVariableIsSet("USERNAME"))
            _usernameEdit->setText(QString::fromLocal8Bit(qgetenv("USERNAME")));

        _usernameEdit->setFocus();
    }
    else
        _passwordEdit->setFocus();

    return QDialog::exec();
}

}   // End of namespace
