// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSet.h>
#include "ModalPropertiesEditorDialog.h"

namespace Ovito {

/******************************************************************************
* The constructor of the dialog.
******************************************************************************/
ModalPropertiesEditorDialog::ModalPropertiesEditorDialog(RefTarget* object, OORef<PropertiesEditor> editor, QWidget* parent, MainWindowUI& userInterface, const QString& dialogTitle, const QString& undoString, const QString& helpTopic) :
    QDialog(parent), _editor(std::move(editor)), UndoableTransaction(userInterface, undoString), UndoSuspender(operation())
{
    setWindowTitle(dialogTitle);

    QVBoxLayout* layout = new QVBoxLayout(this);

    PropertiesPanel* propertiesPanel = new PropertiesPanel(userInterface, this);
    propertiesPanel->setVisible(false);
    _editor->initialize(propertiesPanel, RolloutInsertionParameters().insertInto(this), nullptr);
    _editor->handleExceptions([&]() {
        _editor->setEditObject(object);
    });
    layout->addWidget(propertiesPanel, 1);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &ModalPropertiesEditorDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [&]() {
        setFocus(); // Remove focus from child widgets to commit newly entered values in text widgets etc.
        commit();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, [helpTopic, &userInterface]() {
        userInterface.actionManager()->openHelpTopic(helpTopic);
    });
}

}   // End of namespace
