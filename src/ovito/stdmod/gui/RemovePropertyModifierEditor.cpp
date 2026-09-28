// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/stdmod/modifiers/RemovePropertyModifier.h>
#include "RemovePropertyModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RemovePropertyModifierEditor);
SET_OVITO_OBJECT_EDITOR(RemovePropertyModifier, RemovePropertyModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void RemovePropertyModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Remove property"), rolloutParams, "manual:particles.modifiers.remove_property");

    // Create the rollout contents.
    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* pclassUI =
        createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(GenericPropertyModifier::subject), PropertyContainer::OOClass());
    layout->addWidget(new QLabel(tr("Operate on:")));
    layout->addWidget(pclassUI->comboBox());

    layout->addWidget(new QLabel(tr("Properties:")));

    class TableWidget : public QTableView
    {
    public:
        using QTableView::QTableView;
        [[nodiscard]] QSize sizeHint() const override { return {256, 200}; }
    };
    _propertiesBox = new TableWidget();
    auto* model = new ViewModel(this);
    _propertiesBox->setModel(model);
    _propertiesBox->setShowGrid(false);
    _propertiesBox->setSelectionBehavior(QAbstractItemView::SelectRows);
    _propertiesBox->setCornerButtonEnabled(false);
    _propertiesBox->verticalHeader()->hide();
    _propertiesBox->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    _propertiesBox->setSelectionMode(QAbstractItemView::SingleSelection);
    _propertiesBox->setWordWrap(false);
    _propertiesBox->verticalHeader()->setDefaultSectionSize(_propertiesBox->verticalHeader()->minimumSectionSize());
    layout->addWidget(_propertiesBox);

    connect(_propertiesBox, &QTableView::doubleClicked, model, [model](const QModelIndex& index) {
        QVariant value = model->data(index.siblingAtColumn(0), Qt::CheckStateRole);
        model->setData(index.siblingAtColumn(0), (value.toInt() == Qt::Unchecked) ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
    });

    // Update the properties list whenever the modifier changes.
    connect(this, &PropertiesEditor::contentsChanged, this, [this, model]() { model->refresh(); });

    // Update the properties list whenever the pipeline input changes.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, [this, model]() { model->refresh(); });
}

/******************************************************************************
 * Returns the data stored under the given role for the given index.
 ******************************************************************************/
QVariant RemovePropertyModifierEditor::ViewModel::data(const QModelIndex& index, int role) const
{
    if(index.isValid() && index.row() < _propertyNames.size()) {
        if(role == Qt::DisplayRole && index.column() == 0) return _propertyNames[index.row()];
        if(role == Qt::CheckStateRole && index.column() == 0) {
            if(auto* mod = static_object_cast<RemovePropertyModifier>(editor()->editObject()))
                return mod->propertiesToRemove().contains(_propertyNames[index.row()]) ? Qt::Checked : Qt::Unchecked;
        }
    }
    return {};
}

/******************************************************************************
 * Returns the header data for the given section.
 ******************************************************************************/
QVariant RemovePropertyModifierEditor::ViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0) return tr("Property");
    return {};
}

/******************************************************************************
 * Returns the item flags for the given index.
 ******************************************************************************/
Qt::ItemFlags RemovePropertyModifierEditor::ViewModel::flags(const QModelIndex& index) const
{
    if(index.column() == 0) return QAbstractTableModel::flags(index) | Qt::ItemIsUserCheckable;
    return QAbstractTableModel::flags(index);
}

/******************************************************************************
 * Sets the role data for the item at index to value.
 ******************************************************************************/
bool RemovePropertyModifierEditor::ViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(index.isValid() && index.row() < _propertyNames.size() && role == Qt::CheckStateRole && index.column() == 0) {
        if(auto* mod = static_object_cast<RemovePropertyModifier>(editor()->editObject())) {
            QStringList props = mod->propertiesToRemove();
            const QString& name = _propertyNames[index.row()];
            if(value.toInt() == Qt::Checked)
                props.append(name);
            else
                props.removeAll(name);
            editor()->performTransaction(tr("Select properties to remove"), [&]() { mod->setPropertiesToRemove(std::move(props)); });
            return true;
        }
    }
    return QAbstractItemModel::setData(index, value, role);
}

/******************************************************************************
 * Updates the contents of the model from the current pipeline input.
 ******************************************************************************/
void RemovePropertyModifierEditor::ViewModel::refresh()
{
    beginResetModel();
    _propertyNames.clear();
    if(auto* mod = static_object_cast<RemovePropertyModifier>(editor()->editObject())) {
        if(mod->subject()) {
            for(const PipelineFlowState& inputState : editor()->getPipelineInputs()) {
                if(const PropertyContainer* container = inputState.getLeafObject(mod->subject())) {
                    for(const auto& p : container->properties()) {
                        if(!_propertyNames.contains(p->name())) _propertyNames.append(p->name());
                    }
                    break;
                }
            }
        }
    }
    endResetModel();
}

}  // namespace Ovito
