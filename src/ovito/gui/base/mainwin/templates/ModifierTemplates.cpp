// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/Application.h>
#include "ModifierTemplates.h"

namespace Ovito {

/******************************************************************************
* Returns the singleton instance of this class.
******************************************************************************/
ModifierTemplates* ModifierTemplates::get()
{
    static ModifierTemplates* instance = new ModifierTemplates(Application::instance());
    return instance;
}

/******************************************************************************
* Constructor.
******************************************************************************/
ModifierTemplates::ModifierTemplates(QObject* parent) : ObjectTemplates(QStringLiteral("core/modifier/templates/"), tr("Modifier"), parent)
{
}

}   // End of namespace
