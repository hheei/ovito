// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertyParameterUI.h>
#include <ovito/gui/desktop/widgets/general/StableComboBox.h>

namespace Ovito {

/**
 * \brief UI component for selecting the data object from a data collection.
 */
class OVITO_GUI_EXPORT DataObjectReferenceParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(DataObjectReferenceParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const DataObject::OOMetaClass& dataObjectClass);

    /// Destructor.
    ~DataObjectReferenceParameterUI();

    /// This returns the combobox widget managed by this ParameterUI.
    StableComboBox* comboBox() const { return _comboBox; }

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// Returns the type of data object that can be selected.
    DataObjectClassPtr dataObjectClass() const { return _dataObjectClass; }

    /// Sets the tooltip text for the combo box widget.
    void setToolTip(const QString& text) const {
        if(comboBox()) comboBox()->setToolTip(text);
    }

    /// Sets the What's This helper text for the combo box.
    void setWhatsThis(const QString& text) const {
        if(comboBox()) comboBox()->setWhatsThis(text);
    }

    /// Installs an optional callback function for filtering the displayed object list.
    void setObjectFilter(fu2::unique_function<bool(const ConstDataObjectPath&)>&& filter) {
        _objectFilter = std::move(filter);
        updateUI();
    }

    /// Installs an optional callback function for filtering the displayed object list.
    template<std::derived_from<DataObject> DataObjectClass, std::invocable<const DataObjectClass*> F>
    void setObjectFilter(F&& filter) {
        OVITO_ASSERT(_dataObjectClass->isDerivedFrom(DataObjectClass::OOClass()));
        setObjectFilter([filter=std::forward<F>(filter)](const ConstDataObjectPath& path) {
            if(const DataObjectClass* obj = path.lastAs<DataObjectClass>())
                return std::invoke(filter, obj);
            else
                return false;
        });
    }

public:

    Q_PROPERTY(StableComboBox comboBox READ comboBox)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The combo-box widget.
    QPointer<StableComboBox> _comboBox;

    /// The type of data object that can be selected.
    DataObjectClassPtr _dataObjectClass;

    /// An optional callback function that allows clients to filter the displayed object list.
    fu2::unique_function<bool(const ConstDataObjectPath&)> _objectFilter;
};

}   // End of namespace
