// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/Application.h>
#include "OverlayTemplates.h"

namespace Ovito {

/******************************************************************************
* Returns the singleton instance of this class.
******************************************************************************/
OverlayTemplates* OverlayTemplates::get()
{
    static OverlayTemplates* instance = new OverlayTemplates(Application::instance());
    return instance;
}

/******************************************************************************
* Constructor.
******************************************************************************/
OverlayTemplates::OverlayTemplates(QObject* parent) : ObjectTemplates(QStringLiteral("core/overlay/templates/"), tr("Viewport layer"), parent)
{
}

}   // End of namespace
