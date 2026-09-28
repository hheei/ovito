// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/io/FileImporter.h>

namespace Ovito {

/**
 * This dialog asks the user for a username/password for a remote server.
 */
class OVITO_GUI_EXPORT RemoteAuthenticationDialog : public QDialog
{
    Q_OBJECT

public:

    /// \brief Constructs the dialog window.
    RemoteAuthenticationDialog(QWidget* parent, const QString& title, const QString& labelText);

    /// \brief Sets the username shown in the dialog.
    void setUsername(const QString& username) { _usernameEdit->setText(username); }

    /// \brief Sets the password shown in the dialog.
    void setPassword(const QString& password) { _passwordEdit->setText(password); }

    /// \brief Returns the username entered by the user.
    QString username() const { return _usernameEdit->text(); }

    /// \brief Returns the password entered by the user.
    QString password() const { return _passwordEdit->text(); }

    /// \brief Displays the dialog.
    virtual int exec() override;

private:

    QLineEdit* _usernameEdit;
    QLineEdit* _passwordEdit;
};

}   // End of namespace


