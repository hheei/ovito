// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/app/ApplicationService.h>

namespace Ovito {

/**
 * \brief Application service that makes the Qt Quick frontend selectable.
 *
 * The service is instantiated by the application (like every other ApplicationService of every loaded plugin) and
 * registers the QML frontend with the GuiFrontendRegistry before the application starts its user interface, which is
 * the point at which the frontend selected with --gui is looked up.
 */
class OVITO_GUIQML_EXPORT QmlFrontendService : public ApplicationService
{
    OVITO_CLASS(QmlFrontendService)

public:

    /// Registers the QML frontend.
    virtual void applicationInitializing() override;
};

}   // End of namespace
