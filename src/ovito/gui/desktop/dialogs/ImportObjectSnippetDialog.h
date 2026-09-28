// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/desktop/widgets/display/StatusWidget.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/**
 * This dialog box lets the user import objects from a text snippet.
 */
class ImportObjectSnippetDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ImportObjectSnippetDialog(const QString& objectType, MainWindowUI& ui, QWidget* parentWindow = nullptr);

    /// Returns the list of deserialized objects.
    const std::vector<OORef<RefTarget>>& objects() const { return _objects; }

private Q_SLOTS:

    /// Is called when the snippet text changes.
    void onSnippetTextChanged();

    /// Is called when the user selects an object in the list.
    void onObjectSelectionChanged();

    /// Is called when the user presses the 'Import' button.
    void onImport();

private:

    /// Parses the snippet text and deserializes the objects.
    void parseSnippet(const QString& snippetText);

    /// The text input field for the snippet.
    QPlainTextEdit* _snippetInput;

    /// The status widget displaying parsing/validation errors.
    StatusWidget* _statusWidget;

    /// The list widget displaying the deserialized objects.
    QListWidget* _objectListWidget;

    /// The properties panel displaying the selected object's parameters.
    PropertiesPanel* _propertiesPanel;

    /// The import button.
    QPushButton* _importButton;

    /// The list of deserialized objects.
    std::vector<OORef<RefTarget>> _objects;

    /// The type of object being imported (for display purposes).
    QString _objectType;
};

}	// End of namespace
