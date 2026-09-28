// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/base/app/GuiFrontendRegistry.h>
#include "QmlFrontend.h"
#include "QmlFrontendService.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QmlFrontendService);

/******************************************************************************
* Registers the QML frontend.
******************************************************************************/
void QmlFrontendService::applicationInitializing()
{
    OVITO_ASSERT(this_task::isMainThread());
    GuiFrontendRegistry::instance().registerFrontend(std::make_unique<QmlFrontend>());
}

}   // End of namespace
