// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the ElementType class.
 */
class ElementTypeEditor : public PropertiesEditor
{
    OVITO_CLASS(ElementTypeEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private Q_SLOTS:

    /// Saves the current settings as defaults for the element type.
    void onSaveAsDefault();

private:

    QLabel* _numericIdLabel;
    QPushButton* _setAsDefaultBtn;
    StringParameterUI* _namePUI;
};

}   // End of namespace
