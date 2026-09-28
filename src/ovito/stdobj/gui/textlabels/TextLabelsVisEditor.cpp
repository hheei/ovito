// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/textlabels/TextLabelsVis.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FontParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include "TextLabelsVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TextLabelsVisEditor);
SET_OVITO_OBJECT_EDITOR(TextLabelsVis, TextLabelsVisEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void TextLabelsVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Text labels"), rolloutParams, "manual:visual_elements.text_labels");

    QVBoxLayout* parentLayout = new QVBoxLayout(rollout);
    parentLayout->setContentsMargins(4,4,4,4);
    parentLayout->setSpacing(4);

    // Positioning of the labels relative to the anchor point of their data element.
    QGroupBox* positionBox = new QGroupBox(tr("Positioning"));
    QGridLayout* positionLayout = new QGridLayout(positionBox);
    positionLayout->setContentsMargins(4,4,4,4);
    positionLayout->setColumnStretch(1, 1);
    positionLayout->setColumnStretch(2, 1);
    positionLayout->setSpacing(2);
    positionLayout->setHorizontalSpacing(4);
    parentLayout->addWidget(positionBox);

    VariantComboBoxParameterUI* alignmentPUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TextLabelsVis::alignment));
    positionLayout->addWidget(new QLabel(tr("Alignment:")), 0, 0);
    positionLayout->addWidget(alignmentPUI->comboBox(), 0, 1, 1, 2);
    alignmentPUI->comboBox()->addItem(
        QIcon::fromTheme("overlay_alignment_center"), tr("Center"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_left"), tr("Top left"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top"), tr("Top"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_right"), tr("Top right"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_right"), tr("Right"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_right"), tr("Bottom right"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom"), tr("Bottom"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_left"), tr("Bottom left"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(
        QIcon::fromTheme("overlay_alignment_left"), tr("Left"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignLeft)));

    // The anchor point along an elongated data element. Hidden for all other container types.
    VariantComboBoxParameterUI* elementAnchorPUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TextLabelsVis::elementAnchor));
    _elementAnchorLabel = new QLabel(tr("Anchor:"));
    _elementAnchorBox = elementAnchorPUI->comboBox();
    positionLayout->addWidget(_elementAnchorLabel, 1, 0);
    positionLayout->addWidget(_elementAnchorBox, 1, 1, 1, 2);
    _elementAnchorBox->addItem(tr("Base"), QVariant::fromValue<int>(TextLabelsVis::Base));
    _elementAnchorBox->addItem(tr("Center"), QVariant::fromValue<int>(TextLabelsVis::Center));
    _elementAnchorBox->addItem(tr("Head"), QVariant::fromValue<int>(TextLabelsVis::Head));

    FloatParameterUI* offsetXPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelsVis::offsetX));
    positionLayout->addWidget(new QLabel(tr("XY offset:")), 2, 0);
    positionLayout->addLayout(offsetXPUI->createFieldLayout(), 2, 1);
    FloatParameterUI* offsetYPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelsVis::offsetY));
    positionLayout->addLayout(offsetYPUI->createFieldLayout(), 2, 2);

    FloatParameterUI* depthOffsetPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelsVis::depthOffset));
    positionLayout->addWidget(depthOffsetPUI->label(), 3, 0);
    positionLayout->addLayout(depthOffsetPUI->createFieldLayout(), 3, 1);

    BooleanParameterUI* alwaysInFrontPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TextLabelsVis::alwaysInFront));
    positionLayout->addWidget(alwaysInFrontPUI->checkBox(), 3, 2, 1, 3);

    // The depth offset lifts a label out of its glyph, which is moot while the labels are
    // snapped in front of the entire scene - the two settings are mutually exclusive.
    connect(this, &PropertiesEditor::contentsChanged, this, [depthOffsetPUI](RefTarget* editObject) {
        if(TextLabelsVis* vis = dynamic_object_cast<TextLabelsVis>(editObject)) depthOffsetPUI->setEnabled(!vis->alwaysInFront());
    });

    // The anchor selection follows the kind of data elements the labels are attached to.
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &TextLabelsVisEditor::updateAnchorVisibility);
    connect(this, &PropertiesEditor::contentsChanged, this, [this](RefTarget* editObject) { updateAnchorVisibility(); });
    updateAnchorVisibility();

    // Appearance of the labels.
    QGroupBox* styleBox = new QGroupBox(tr("Style"));
    QGridLayout* styleLayout = new QGridLayout(styleBox);
    styleLayout->setContentsMargins(4,4,4,4);
    styleLayout->setColumnStretch(1, 1);
    styleLayout->setSpacing(2);
    styleLayout->setHorizontalSpacing(4);
    parentLayout->addWidget(styleBox);

    int row = 0;

    FontParameterUI* fontPUI = createParamUI<FontParameterUI>(PROPERTY_FIELD(TextLabelsVis::font));
    styleLayout->addWidget(fontPUI->label(), row, 0);
    styleLayout->addWidget(fontPUI->fontPicker(), row++, 1);

    FloatParameterUI* fontSizePUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelsVis::fontSize));
    styleLayout->addWidget(new QLabel(tr("Font size:")), row, 0);
    styleLayout->addLayout(fontSizePUI->createFieldLayout(), row++, 1);

    ColorParameterUI* textColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TextLabelsVis::textColor));
    styleLayout->addWidget(new QLabel(tr("Color:")), row, 0);
    styleLayout->addWidget(textColorPUI->colorPicker(), row++, 1);

    BooleanParameterUI* outlineEnabledPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TextLabelsVis::outlineEnabled));
    styleLayout->addWidget(outlineEnabledPUI->checkBox(), row, 0);
    outlineEnabledPUI->checkBox()->setText(tr("Outline:"));
    ColorParameterUI* outlineColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TextLabelsVis::outlineColor));
    styleLayout->addWidget(outlineColorPUI->colorPicker(), row++, 1);

    BooleanParameterUI* backgroundEnabledPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TextLabelsVis::backgroundEnabled));
    styleLayout->addWidget(backgroundEnabledPUI->checkBox(), row, 0);
    backgroundEnabledPUI->checkBox()->setText(tr("Background:"));
    ColorParameterUI* backgroundColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TextLabelsVis::backgroundColor));
    styleLayout->addWidget(backgroundColorPUI->colorPicker(), row++, 1);

    // Advanced settings of the labels.
    QGroupBox* advancedBox = new QGroupBox(tr("Advanced"));
    QGridLayout* advancedLayout = new QGridLayout(advancedBox);
    advancedLayout->setContentsMargins(4, 4, 4, 4);
    advancedLayout->setColumnStretch(1, 1);
    advancedLayout->setSpacing(2);
    advancedLayout->setHorizontalSpacing(4);
    parentLayout->addWidget(advancedBox);

    row = 0;

    IntegerParameterUI* maxLabelCountPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TextLabelsVis::maxLabelCount));
    advancedLayout->addWidget(maxLabelCountPUI->label(), row, 0);
    advancedLayout->addLayout(maxLabelCountPUI->createFieldLayout(), row++, 1);
}

/******************************************************************************
 * Updates the visibility of the anchor selection box.
 ******************************************************************************/
void TextLabelsVisEditor::updateAnchorVisibility()
{
    // The anchor point can be selected only for elongated data elements such as bonds or vectors.
    // The vis element may be attached to the data of more than one pipeline. Showing the box if any
    // of them provides elongated elements is preferable over hiding a setting that is in effect.
    bool visible = false;
    for(const ConstDataObjectRefPath& path : getVisDataObjectPaths()) {
        // The data object path ends in the labeled property, so its container is the predecessor.
        if(const PropertyContainer* propertyContainer = path.nextToLastAs<PropertyContainer>()) {
            if(propertyContainer->getOOMetaClass().supportLabelAnchors()) {
                visible = true;
                break;
            }
        }
    }

    _elementAnchorLabel->setVisible(visible);
    _elementAnchorBox->setVisible(visible);
    container()->updateRolloutsLater();
}

}  // namespace Ovito
