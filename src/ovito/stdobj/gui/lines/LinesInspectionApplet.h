// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>

namespace Ovito {

/**
 * \brief Data inspector page for lines.
 */
class OVITO_STDOBJGUI_EXPORT LinesInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(LinesInspectionApplet)

public:

    /// Constructor.
    void initializeObject() { PropertyInspectionApplet::initializeObject(Lines::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 250; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;
};

}  // namespace Ovito
