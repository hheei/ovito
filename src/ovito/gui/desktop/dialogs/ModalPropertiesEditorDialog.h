// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/**
 * A dialog box displaying a PropertiesEditor for an object.
 */
class ModalPropertiesEditorDialog : public QDialog, private UndoableTransaction, private UndoSuspender
{
    Q_OBJECT

public:

    /// Constructor.
    ModalPropertiesEditorDialog(RefTarget* object, OORef<PropertiesEditor> editor, QWidget* parentWindow, MainWindowUI& userInterface, const QString& dialogTitle, const QString& undoString, const QString& helpTopic);

private:

    OORef<PropertiesEditor> _editor;
};

}   // End of namespace
