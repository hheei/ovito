// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "FontSelectionDialog.h"

namespace Ovito {

/******************************************************************************
* The constructor of the dialog.
******************************************************************************/
FontSelectionDialog::FontSelectionDialog(QWidget* parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Select font"));

    (void)new QVBoxLayout(this);
}

/******************************************************************************
* Shows a dialog that lets the pick a font.
******************************************************************************/
QFont FontSelectionDialog::getFont(bool* ok, QFont currentFont, QWidget* parentWindow)
{
    return QFontDialog::getFont(ok, currentFont, parentWindow);
}

}   // End of namespace
