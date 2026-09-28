// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "FloatParameterUI.h"

namespace Ovito {

/******************************************************************************
* A parameter UI for Vector3 properties.
* This ParameterUI lets the user edit one of the X, Y and Z components of the vector.
******************************************************************************/
class OVITO_GUI_EXPORT VectorParameterUI : public FloatParameterUI
{
    OVITO_CLASS(VectorParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t vectorComponentIndex, size_t vectorComponentCount);

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    virtual void updatePropertyValue() override;

private:

    /// The index of the vector component to control (0 - 2).
    size_t _componentIndex;

    /// The vector component count (2 or 3)
    size_t _componentCount;
};

}   // End of namespace
