// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/base/app/GuiFrontend.h>

namespace Ovito {

/**
 * \brief The Qt Quick / QML frontend of OVITO, which presents the workbench as a Qt Quick window.
 *
 * The frontend is selected with the \c --gui=qml command line option and is part of the build only when the
 * OVITO_BUILD_QML_FRONTEND option is enabled. It is registered with the GuiFrontendRegistry by QmlFrontendService.
 */
class OVITO_GUIQML_EXPORT QmlFrontend : public GuiFrontend
{
public:

    /// Returns the name under which this frontend is selected on the command line.
    virtual QString name() const override;

    /// Returns a human-readable description of this frontend.
    virtual QString description() const override;

    /// Creates the Qt Quick workbench and its user interface object.
    virtual MainThreadOperation createWorkbench() const override;
};

}   // End of namespace
