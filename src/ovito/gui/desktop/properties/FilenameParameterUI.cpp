// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/FilenameParameterUI.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FilenameParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void FilenameParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const QStringList& fileFilter, bool existingFile)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _fileFilter = fileFilter;
    _existingFile = existingFile;

    // Create UI widget.
    _selectorButton = new QPushButton(QStringLiteral(" "));
    _selectorButton->setAccessibleName(propertyField()->displayName());
    connect(_selectorButton.data(), &QPushButton::clicked, this, &FilenameParameterUI::onPickFilename);
}

/******************************************************************************
* Destructor.
******************************************************************************/
FilenameParameterUI::~FilenameParameterUI()
{
    // Release GUI controls.
    delete selectorWidget();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void FilenameParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(selectorWidget())
        selectorWidget()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void FilenameParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(selectorWidget() && editObject()) {
        QVariant val;

        if(propertyField()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }

        QString filename = val.toString();
        if(filename.isEmpty() == false) {
            selectorWidget()->setText(QFileInfo(filename).fileName());
        }
        else {
            selectorWidget()->setText(tr("[Choose File...]"));
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void FilenameParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(selectorWidget())
        selectorWidget()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* Is called when the user presses the button.
******************************************************************************/
void FilenameParameterUI::onPickFilename()
{
    performTransaction(tr("Pick file"), [&]() {
        HistoryFileDialog fileDialog(ui(), QStringLiteral("filename_parameter"), editor()->container(), tr("Pick file"));
        fileDialog.setNameFilters(_fileFilter);
        fileDialog.setFileMode(_existingFile ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
        fileDialog.setAcceptMode(_existingFile ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
        if(fileDialog.exec()) {
            QStringList selectedFiles = fileDialog.selectedFiles();
            if(!selectedFiles.empty()) {
                ProgressDialog::showForCurrentTask(ui(), editor()->parentWindow());
                if(isPropertyFieldUI() && editObject()) {
                    editObject()->setPropertyFieldValue(propertyField(), selectedFiles.join(QDir::listSeparator()));
                }
                Q_EMIT valueEntered();
            }
        }
    });
}

}   // End of namespace
