// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the CoordinateTripodOverlay class.
 */
class CoordinateTripodOverlayEditor : public PropertiesEditor
{
    OVITO_CLASS(CoordinateTripodOverlayEditor)

public:

    /// Constructor.
    explicit CoordinateTripodOverlayEditor() {}

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// The current viewport.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Viewport>, viewport, setViewport);

    BooleanParameterUI* _perspectiveDistortionUI;
};

}   // End of namespace
