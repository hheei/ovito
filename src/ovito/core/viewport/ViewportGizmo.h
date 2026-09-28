// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/**
 * \brief Abstract base class for viewport gizmos that display additional content in the
 *        interactive viewports.
 */
class ViewportGizmo
{
public:

    /// \brief Lets the input mode render its overlay content in a viewport.
    /// \param vp The viewport into which the mode should render its specific overlay content.
    /// \param vpWin The viewport window into which the mode should render its specific overlay content.
    /// \param frameGraph The frame graph to be populated with visual primitives.
    /// \param dataset The data set being visualized in the viewport.
    ///
    /// This method is called by the system every time the viewports are redrawn and this input
    /// mode is on the input mode stack.
    ///
    /// The default implementation of this method does nothing.
    virtual void renderOverlay(Viewport* vp, ViewportWindow* vpWin, FrameGraph& frameGraph, DataSet* dataset) {}
};

}   // End of namespace
