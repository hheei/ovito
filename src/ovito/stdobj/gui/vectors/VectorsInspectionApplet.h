// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/vectors/Vectors.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>

namespace Ovito {
/**
 * \brief Data inspector page for Vectors.
 */
class OVITO_STDOBJGUI_EXPORT VectorsInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(VectorsInspectionApplet)

public:
    /// Constructor.
    void initializeObject() { PropertyInspectionApplet::initializeObject(Vectors::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 300; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;
};

}  // namespace Ovito
