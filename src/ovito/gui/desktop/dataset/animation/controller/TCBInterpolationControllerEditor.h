// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/core/dataset/animation/controller/TCBInterpolationControllers.h>

namespace Ovito {

/**
 * A properties editor template for the TCBAnimationKey class template.
 */
template<class TCBAnimationKeyType>
class TCBAnimationKeyEditor : public PropertiesEditor
{
protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override {
        // Create the rollout.
        QWidget* rollout = createRollout(tr("TCB Animation Key"), rolloutParams);

        QVBoxLayout* layout = new QVBoxLayout(rollout);
        layout->setContentsMargins(4,4,4,4);
        layout->setSpacing(2);

        QGridLayout* sublayout = new QGridLayout();
        sublayout->setContentsMargins(0,0,0,0);
        sublayout->setColumnStretch(2, 1);
        layout->addLayout(sublayout);

        // Ease to parameter.
        FloatParameterUI* easeInPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TCBAnimationKeyType::easeTo));
        sublayout->addWidget(easeInPUI->label(), 0, 0);
        sublayout->addLayout(easeInPUI->createFieldLayout(), 0, 1);

        // Ease from parameter.
        FloatParameterUI* easeFromPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TCBAnimationKeyType::easeFrom));
        sublayout->addWidget(easeFromPUI->label(), 1, 0);
        sublayout->addLayout(easeFromPUI->createFieldLayout(), 1, 1);

        // Tension parameter.
        FloatParameterUI* tensionPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TCBAnimationKeyType::tension));
        sublayout->addWidget(tensionPUI->label(), 2, 0);
        sublayout->addLayout(tensionPUI->createFieldLayout(), 2, 1);

        // Continuity parameter.
        FloatParameterUI* continuityPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TCBAnimationKeyType::continuity));
        sublayout->addWidget(continuityPUI->label(), 3, 0);
        sublayout->addLayout(continuityPUI->createFieldLayout(), 3, 1);

        // Continuity parameter.
        FloatParameterUI* biasPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TCBAnimationKeyType::bias));
        sublayout->addWidget(biasPUI->label(), 4, 0);
        sublayout->addLayout(biasPUI->createFieldLayout(), 4, 1);
    }
};

/**
 * A properties editor for the PositionTCBAnimationKey class.
 */
class PositionTCBAnimationKeyEditor : public TCBAnimationKeyEditor<PositionTCBAnimationKey>
{
    OVITO_CLASS(PositionTCBAnimationKeyEditor)
};

}   // End of namespace


