// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include "DeleteSelectedModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(DeleteSelectedModifierDelegate);
IMPLEMENT_CREATABLE_OVITO_CLASS(DeleteSelectedModifier);
OVITO_CLASSINFO(DeleteSelectedModifier, "DisplayName", "Delete selected");
OVITO_CLASSINFO(DeleteSelectedModifier, "Description", "Remove all currently selected elements.");
OVITO_CLASSINFO(DeleteSelectedModifier, "ModifierCategory", "Modification");

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant DeleteSelectedModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(scene);

    // Note: This method only gets called as long as the modifier's evaluation status carries no short info of its own,
    // i.e. before the first pipeline evaluation and whenever no elements were deleted. See
    // ModificationNode::getPipelineEditorShortInfo(), which gives the status' short info precedence over this method.
    // The delegates report the number of deleted elements through the status' short info.

    // If there is exactly one enabled delegate, use its name as short info.
    QVariant shortInfo;
    for(const auto& delegate : delegates()) {
        if(delegate->isEnabled()) {
            if(!shortInfo.isNull())
                return {};  // More than one enabled delegate -> no short info.
            else
                shortInfo = delegate->objectTitle();
        }
    }
    return shortInfo;
}

}   // End of namespace
