// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the TextLabelOverlay class.
 */
class TextLabelOverlayEditor : public PropertiesEditor
{
    OVITO_CLASS(TextLabelOverlayEditor)

public:

    /// Constructor.
    explicit TextLabelOverlayEditor() {}

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Updates the displayed list of available pipeline variables.
    void updateVariablesList();

private:

    QLabel* _attributeNamesList;
    AutocompleteTextEdit* _textEdit;
};

}   // End of namespace
