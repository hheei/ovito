// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief User interface component for the AttributeFileExporter class.
 */
class AttributeFileExporterEditor : public PropertiesEditor
{
    OVITO_CLASS(AttributeFileExporterEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Rebuilds the displayed list of exportable attributes.
    void updateAttributesList();

    /// Is called when the user checked/unchecked an item in the attributes list.
    void onAttributeChanged();

private:

    /// Populates the column mapping list box with an entry.
    void insertAttributeItem(const QString& displayName, const QStringList& selectedAttributeList);

    QListWidget* _columnMappingWidget;
};

}   // End of namespace
