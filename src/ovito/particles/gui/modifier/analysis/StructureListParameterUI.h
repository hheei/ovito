// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/gui/desktop/properties/RefTargetListParameterUI.h>

namespace Ovito {

/**
 * List box that displays the structure types.
 */
class OVITO_PARTICLESGUI_EXPORT StructureListParameterUI : public RefTargetListParameterUI
{
    OVITO_CLASS(StructureListParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, bool showCheckBoxes = false);

    /// This method is called when a new editable object has been activated.
    virtual void resetUI() override;

    /// Creates a standard QLabel displaying usage instructions for the structure list.
    QLabel* createNotesLabel();

protected:

    /// Returns a data item from the list data model.
    virtual QVariant getItemData(RefTarget* target, const QModelIndex& index, int role) override;

    /// Returns the model/view item flags for the given entry.
    virtual Qt::ItemFlags getItemFlags(RefTarget* target, const QModelIndex& index) override;

    /// Sets the role data for the item at index to value.
    virtual bool setItemData(RefTarget* target, const QModelIndex& index, const QVariant& value, int role) override;

    /// Returns the number of columns for the table view.
    virtual int tableColumnCount() override { return 5; }

    /// Returns the header data under the given role for the given RefTarget.
    virtual QVariant getHorizontalHeaderData(int index, int role) override {
        if(role == Qt::DisplayRole) {
            if(index == 0)
                return QVariant();
            else if(index == 1)
                return QVariant::fromValue(tr("Structure"));
            else if(index == 2)
                return QVariant::fromValue(tr("Count"));
            else if(index == 3)
                return QVariant::fromValue(tr("Fraction"));
            else if(index == 4)
                return QVariant::fromValue(tr("Id"));
        }
        return RefTargetListParameterUI::getHorizontalHeaderData(index, role);
    }

    /// Do not open sub-editor for selected structure type.
    virtual void openSubEditor() override {}

protected Q_SLOTS:

    /// Is called when the user has double-clicked on one of the structure types in the list widget.
    void onDoubleClickStructureType(const QModelIndex& index);

private:

    /// Obtains the current structure counts from the pipeline.
    void updateStructureCounts();

    /// Controls whether a check box is shown next to each structure type.
    bool _showCheckBoxes;

    /// The data array containing the number of identified particles of each structure type.
    ConstPropertyPtr _structureCounts;
};

}   // End of namespace
