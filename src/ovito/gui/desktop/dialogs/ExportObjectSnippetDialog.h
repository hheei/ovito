// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This dialog box generates a text representation of one or more OVITO objects (e.g. modifiers),
 * which can be copied to the clipboard or saved to a file for later reuse and sharing with others.
 */
class ExportObjectSnippetDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ExportObjectSnippetDialog(const std::vector<OORef<RefTarget>>& objects, const QString& snippetDescription, const QString& usageNotice, MainWindowUI& ui, QWidget* parentWindow = nullptr);

private:

    /// Generates the text snippet representing the given list of objects.
    QString generateObjectSnippet(const std::vector<OORef<RefTarget>>& objects, const QString& snippetDescription);
};

}   // End of namespace
