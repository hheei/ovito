// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include "StandardRendererEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(StandardRendererEditor);
SET_OVITO_OBJECT_EDITOR(StandardRenderer, StandardRendererEditor);

/******************************************************************************
* Constructor that creates the UI controls for the editor.
******************************************************************************/
void StandardRendererEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create the rollout.
    QWidget* rollout = createRollout(tr("Standard renderer settings"), rolloutParams, "manual:rendering.standard_renderer");

    // Create the rollout contents.
    QVBoxLayout* rootLayout = new QVBoxLayout(rollout);
    rootLayout->setContentsMargins(4,4,4,4);

    QGroupBox* qualityBox = new QGroupBox(tr("Quality"), rollout);
    rootLayout->addWidget(qualityBox);
    QGridLayout* gridLayout = new QGridLayout(qualityBox);
    gridLayout->setContentsMargins(4,4,4,4);
#ifndef Q_OS_MACOS
    gridLayout->setSpacing(2);
#endif
    gridLayout->setColumnStretch(1, 1);

    // Antialiasing level
    IntegerParameterUI* antialiasingLevelUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(StandardRenderer::antialiasingLevel));
    gridLayout->addWidget(antialiasingLevelUI->label(), 0, 0);
    gridLayout->addLayout(antialiasingLevelUI->createFieldLayout(), 0, 1);

    // Transparency rendering method
    QGroupBox* transparencyBox = new QGroupBox(tr("Transparency rendering method"), rollout);
    rootLayout->addWidget(transparencyBox);
    QHBoxLayout* boxLayout = new QHBoxLayout(transparencyBox);
    boxLayout->setContentsMargins(4,4,4,4);

    VariantComboBoxParameterUI* transparencyMethodUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(StandardRenderer::orderIndependentTransparency));
    transparencyMethodUI->comboBox()->addItem(tr("Back-to-Front Ordered (default)"), QVariant::fromValue(false));
    transparencyMethodUI->comboBox()->addItem(tr("Weighted Blended Order-Independent"), QVariant::fromValue(true));
    boxLayout->addWidget(transparencyMethodUI->comboBox());

    // Settings management functions.
    rootLayout->addWidget(createCopySettingsBetweenRenderersWidget());

    // Conditionally hide the "Quality" group box if this editor is for the interactive viewport renderer and not the final frame renderer.
    connect(this, &BaseSceneRendererEditor::editingInteractiveRenderer, qualityBox, &QWidget::hide);
}

/******************************************************************************
* Copies the settings of one renderer to another (which can either be an interactive or a final-frame renderer).
******************************************************************************/
void StandardRendererEditor::transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final)
{
    StandardRenderer* sourceRenderer = dynamic_object_cast<StandardRenderer>(source);
    StandardRenderer* targetRenderer = dynamic_object_cast<StandardRenderer>(target);
    if(sourceRenderer && targetRenderer) {
        targetRenderer->setOrderIndependentTransparency(sourceRenderer->orderIndependentTransparency());
    }
}

}   // End of namespace
