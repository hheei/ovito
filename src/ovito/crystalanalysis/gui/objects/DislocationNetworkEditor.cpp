// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/gui/desktop/properties/RefTargetListParameterUI.h>
#include "DislocationNetworkEditor.h"
#include "MicrostructurePhaseEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationNetworkEditor);
SET_OVITO_OBJECT_EDITOR(DislocationNetwork, DislocationNetworkEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void DislocationNetworkEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Dislocations"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    // Derive a custom class from the list parameter UI to
    // give the items a color.
    class CustomRefTargetListParameterUI : public RefTargetListParameterUI {
    public:

        void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* refField, const RolloutInsertionParameters& rolloutParams) {
            RefTargetListParameterUI::initializeObject(parentEditor, refField, rolloutParams, &MicrostructurePhaseEditor::OOClass());
        }

    protected:

        virtual QVariant getItemData(RefTarget* target, const QModelIndex& index, int role) override {
            if(target) {
                if(role == Qt::DisplayRole) {
                    if(index.column() == 1)
                        return target->objectTitle();
                }
                else if(role == Qt::DecorationRole) {
                    if(index.column() == 0)
                        return (QColor)static_object_cast<MicrostructurePhase>(target)->color();
                }
            }
            return {};
        }

        /// Returns the number of columns for the table view.
        virtual int tableColumnCount() override { return 2; }

        /// Returns the header data under the given role for the given RefTarget.
        /// This method is part of the data model used by the list widget and can be overridden
        /// by sub-classes.
        virtual QVariant getHorizontalHeaderData(int index, int role) override {
            if(index == 0)
                return tr("Color");
            else
                return tr("Name");
        }
    };

    QWidget* subEditorContainer = new QWidget(rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(subEditorContainer);
    sublayout->setContentsMargins(0,0,0,0);
    layout->addWidget(subEditorContainer);

    layout->insertWidget(0, new QLabel(tr("Structure types:")));
    typesListUI = createParamUI<CustomRefTargetListParameterUI>(PROPERTY_FIELD(DislocationNetwork::crystalStructures), RolloutInsertionParameters().insertInto(subEditorContainer));
    layout->insertWidget(1, typesListUI->tableWidget(200));
    typesListUI->tableWidget()->setAutoScroll(false);
    connect(typesListUI->tableWidget(), &QTableWidget::doubleClicked, this, &DislocationNetworkEditor::onDoubleClickPattern);
}

/******************************************************************************
* Is called when the user has double-clicked on one of the entries in the
* list widget.
******************************************************************************/
void DislocationNetworkEditor::onDoubleClickPattern(const QModelIndex& index)
{
    // Let the user select a color for the structure.
    ElementType* pattern = static_object_cast<ElementType>(typesListUI->selectedObject());
    if(!pattern) return;

    QColor oldColor = Color(pattern->color());
    QColor newColor = QColorDialog::getColor(oldColor, container());
    if(!newColor.isValid() || newColor == oldColor)
        return;

    performTransaction(tr("Change structure type color"), [pattern, &newColor]() {
        pattern->setColor(Color(newColor));
    });
}

}   // End of namespace
