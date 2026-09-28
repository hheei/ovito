// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTargetListener.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(RefTargetListenerImpl);
DEFINE_REFERENCE_FIELD(RefTargetListenerImpl, target);

IMPLEMENT_ABSTRACT_OVITO_CLASS(VectorRefTargetListenerImpl);
DEFINE_VECTOR_REFERENCE_FIELD(VectorRefTargetListenerImpl, targets);

/******************************************************************************
* Is called when the RefTarget referenced by this listener has sent a message.
******************************************************************************/
bool RefTargetListenerImpl::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    // Emit Qt signal.
    Q_EMIT notificationEvent(source, event);

    return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the RefTarget referenced by this listener has sent a message.
******************************************************************************/
bool VectorRefTargetListenerImpl::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    // Emit Qt signal.
    Q_EMIT notificationEvent(source, event);

    return RefMaker::referenceEvent(source, event);
}

}   // End of namespace
