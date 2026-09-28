// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * Abstract base for editor components for SceneRenderer classes.
 */
class OVITO_GUI_EXPORT BaseSceneRendererEditor : public PropertiesEditor
{
    OVITO_CLASS(BaseSceneRendererEditor)
    Q_OBJECT

public:

    /// Constructor.
    BaseSceneRendererEditor();

    /// Creates an action widget that lets the user copy the settings of the current interactive renderer to/from the final-frame renderer.
    QWidget* createCopySettingsBetweenRenderersWidget(QWidget* parent = nullptr);

    /// Determines whether the settings of the given scene renderers can be transferred between each other.
    virtual bool canTransferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target) {
        return &source->getOOClass() == &target->getOOClass();
    }

    /// Copies the settings of one renderer to another (which can either be an interactive or a final-frame renderer).
    virtual void transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final) {}

public Q_SLOTS:

    /// Copies the settings of the interactive renderer to the final-frame renderer.
    void copySettingsInteractiveToFinalFrame();

    /// Copies the settings of the final-frame renderer to the interactive renderer.
    void copySettingsFinalFrameToInteractive();

Q_SIGNALS:

    /// This signal is emitted when the editor loads a scene renderer that is being used for interactive viewport rendering.
    void editingInteractiveRenderer();

    /// This signal is emitted when the editor loads a scene renderer that is being used for final frame rendering.
    void editingFinalFrameRenderer();
};

}   // End of namespace
