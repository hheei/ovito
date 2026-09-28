// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/widgets/general/MenuToolButton.h>

namespace Ovito {

/**
 * \brief A properties editor for the ParticleType class.
 */
class ParticleTypeEditor : public PropertiesEditor
{
    OVITO_CLASS(ParticleTypeEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Creates a button that opens a menu for managing the presets for a particle type parameter.
    QToolButton* createPresetsMenuButton(const QString& parameterName, std::function<void(ParticleType*)> resetFunc, std::function<void(const ParticleType*)> setDefaultFunc, std::function<bool(const ParticleType*)> isUnchangedFunc);

private Q_SLOTS:

    /// Called when the contents of the editor have been replaced with a new object.
    void onContentsReplaced();

    /// Called when the user wants to pick and load a mesh-based particle shape from disk.
    void onLoadParticleShape();

private:

    QToolButton* _colorPresetsMenuButton;
    QToolButton* _displayRadiusPresetsMenuButton;
    QToolButton* _vdwRadiusPresetsMenuButton;
    MenuToolButton* _massPresetsMenuButton;
    QPushButton* _loadShapeBtn;
};

}   // End of namespace
