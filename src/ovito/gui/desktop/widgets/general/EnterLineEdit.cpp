// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "EnterLineEdit.h"

namespace Ovito {

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void EnterLineEdit::keyPressEvent(QKeyEvent* event)
{
    QLineEdit::keyPressEvent(event);

    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        event->accept();
}

}   // End of namespace
