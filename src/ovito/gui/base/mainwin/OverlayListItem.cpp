// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/viewport/Viewport.h>
#include "OverlayListItem.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OverlayListItem);
DEFINE_REFERENCE_FIELD(OverlayListItem, overlay);

/******************************************************************************
* Constructor.
******************************************************************************/
void OverlayListItem::initializeObject(ViewportOverlay* overlay, OverlayItemType itemType)
{
    RefMaker::initializeObject();

    _itemType = itemType;
    _overlay.set(this, PROPERTY_FIELD(overlay), overlay);
}

/******************************************************************************
* This method is called when the object presented by the modifier
* list item generates a message.
******************************************************************************/
bool OverlayListItem::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    /// Update item if it has been enabled/disabled, its status has changed, or its title has changed.
    if(event.type() == ReferenceEvent::TargetEnabledOrDisabled || event.type() == ReferenceEvent::ObjectStatusChanged || event.type() == ReferenceEvent::TitleChanged) {
        Q_EMIT itemChanged(this);
    }

    return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Returns the status of the object represented by the list item.
******************************************************************************/
const PipelineStatus& OverlayListItem::status() const
{
    if(overlay()) {
        return overlay()->status();
    }
    else {
        static const PipelineStatus defaultStatus;
        return defaultStatus;
    }
}

/******************************************************************************
* Returns the text for this list item.
******************************************************************************/
QString OverlayListItem::title(Viewport* selectedViewport) const
{
    OVITO_ASSERT(selectedViewport);
    switch(_itemType) {
    case Layer:
        return overlay() ? overlay()->objectTitle() : QString();
    case ViewportHeader: return tr("Active viewport: %1").arg(selectedViewport->viewportTitle());
    case SceneLayer: return tr("3D scene layer");
    default: return {};
    }
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant OverlayListItem::shortInfo(Viewport* selectedViewport) const
{
    OVITO_ASSERT(this_task::get());
    if(overlay()) {
        if(Scene* scene = selectedViewport->scene()) {
            return overlay()->getPipelineEditorShortInfo(scene);
        }
    }
    return {};
}

}   // End of namespace
