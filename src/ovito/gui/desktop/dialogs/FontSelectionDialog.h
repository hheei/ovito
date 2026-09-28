// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This dialog box lets the user select a font.
 */
class OVITO_GUI_EXPORT FontSelectionDialog : public QDialog
{
    Q_OBJECT

public:

    /// Constructor.
    FontSelectionDialog(QWidget* parentWindow = nullptr);

    /// Shows a dialog that lets the pick a font.
    static QFont getFont(bool* ok, QFont currentFont, QWidget* parentWindow);

private:

};

}   // End of namespace


