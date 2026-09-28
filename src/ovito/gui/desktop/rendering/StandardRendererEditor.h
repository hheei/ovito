// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/rendering/BaseSceneRendererEditor.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/******************************************************************************
* The editor component for the StandardRenderer class.
******************************************************************************/
class StandardRendererEditor : public BaseSceneRendererEditor
{
    OVITO_CLASS(StandardRendererEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Copies the settings of one renderer to another (which can either be an interactive or a final-frame renderer).
    virtual void transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final) override;
};

}   // End of namespace
