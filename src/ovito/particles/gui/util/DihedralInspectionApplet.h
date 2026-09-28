// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/objects/Dihedrals.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>

namespace Ovito {


/**
 * \brief Data inspector page for the list of molecular dihedrals.
 */
class DihedralInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(DihedralInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject() { PropertyInspectionApplet::initializeObject(Dihedrals::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 16; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;
};

}   // End of namespace
