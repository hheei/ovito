// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/viewport/Viewport.h>

namespace Ovito {

/**
 * \brief Utility class that supports the picking of bonds in the viewports.
 */
class OVITO_PARTICLESGUI_EXPORT BondPickingHelper
{
public:

    struct PickResult {

        /// The index of the picked bond.
        size_t bondIndex;

        /// The pipeline scene node pipeline that produced the picked bond.
        OORef<SceneNode> sceneNode;
    };

    /// \brief Finds the bond under the mouse cursor.
    /// \param vpwin The viewport window to perform hit testing in.
    /// \param clickPoint The position of the mouse cursor in the viewport.
    /// \param time The animation at which hit testing is performed.
    /// \param result The output structure that receives information about the picked bond.
    /// \return \c true if a bond has been picked; \c false otherwise.
    bool pickBond(ViewportWindow* vpwin, const QPoint& clickPoint, PickResult& result);
};

}   // End of namespace
