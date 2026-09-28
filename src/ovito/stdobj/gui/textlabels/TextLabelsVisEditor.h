// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the TextLabelsVis class.
 */
class OVITO_STDOBJGUI_EXPORT TextLabelsVisEditor : public PropertiesEditor
{
    OVITO_CLASS(TextLabelsVisEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Updates the visibility of the anchor selection box, which applies to elongated data
    /// elements such as bonds and vectors only.
    void updateAnchorVisibility();

    /// Selection box for the anchor point along an elongated data element.
    QComboBox* _elementAnchorBox;

    /// The caption of the anchor selection box. Both are shown only for those input containers
    /// whose data elements are elongated.
    QLabel* _elementAnchorLabel;
};

}  // namespace Ovito
