// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This dialog box shows information about the user's system, e.g., which GPU adapters are available.
 */
class SystemInformationDialog : public QDialog
{
    Q_OBJECT

public:

    /// Constructor.
    explicit SystemInformationDialog(UserInterface& userInterface, QWidget* parentWindow = nullptr);
};

}   // End of namespace
