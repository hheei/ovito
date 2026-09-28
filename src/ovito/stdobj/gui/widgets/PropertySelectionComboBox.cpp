// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/PropertyContainerClass.h>
#include "PropertySelectionComboBox.h"

namespace Ovito {

/******************************************************************************
* Returns the property that is currently selected in the combo box.
******************************************************************************/
PropertyReference PropertySelectionComboBox::currentProperty() const
{
    OVITO_ASSERT(_newItems.empty());
    if(!isEditable()) {
        int index = currentIndex();
        if(index < 0)
            return PropertyReference();
        return itemData(index).value<PropertyReference>();
    }
    else {
        return currentText().simplified();
    }
}

/******************************************************************************
* Sets the selection of the combo box to the given property.
******************************************************************************/
void PropertySelectionComboBox::setCurrentProperty(const PropertyReference& property)
{
    OVITO_ASSERT(_newItems.empty());
    int index = findData(QVariant::fromValue(property));
    if(index >= 0) {
        setCurrentIndex(index);
    }
    else {
        if(isEditable() && property) {
            setCurrentText(property.nameWithComponent());
        }
        else {
            setCurrentIndex(-1);
        }
    }
}

/******************************************************************************
* Is called when the widget loses the input focus.
******************************************************************************/
void PropertySelectionComboBox::focusOutEvent(QFocusEvent* event)
{
    if(isEditable()) {
        int index = findText(currentText());
        if(index == -1 && currentText().isEmpty() == false) {
            OVITO_ASSERT(_newItems.empty());
            addItem(PropertyReference(currentText()));
            OVITO_ASSERT(_newItems.size() == 1);
            qobject_cast<QStandardItemModel*>(this->model())->appendRow(_newItems.back().release());
            _newItems.clear();
            index = count() - 1;
        }
        setCurrentIndex(index);
        Q_EMIT activated(index);
        Q_EMIT textActivated(currentText());
    }
    QComboBox::focusOutEvent(event);
}

}   // End of namespace
