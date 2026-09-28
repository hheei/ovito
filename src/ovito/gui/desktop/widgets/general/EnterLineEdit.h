// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A variant of the QLineEdit standard widget which intercepts 'Enter' key-press events instead of forwarding them to the parent widget.
 *        This is used in places where the edit widget is a child of a checkable QGroupBox, in which
 *        the enter key otherwise would toggle the group widget's checkbox.
 */
class OVITO_GUI_EXPORT EnterLineEdit : public QLineEdit
{
    Q_OBJECT

public:

    /// Constructor.
    using QLineEdit::QLineEdit;

protected:

    /// Handles key-press events.
    virtual void keyPressEvent(QKeyEvent* event) override;
};

}   // End of namespace
