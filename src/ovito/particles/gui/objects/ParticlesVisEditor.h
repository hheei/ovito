// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the ParticlesVis class.
 */
class ParticlesVisEditor : public PropertiesEditor
{
    OVITO_CLASS(ParticlesVisEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Enables or disables UI options depending on the current pipeline input.
    void updateOptions();

    /// Handles actions links in the editor UI, e.g., by inserting the specified modifier into the current pipeline.
    void handleActionLinks(const QString& action);

private:

    FloatParameterUI* _defaultRadiusUI;
    VariantComboBoxParameterUI* _particleShapeUI;
};

}   // End of namespace
