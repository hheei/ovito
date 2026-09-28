// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FontParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/VectorParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/PipelineSelectionParameterUI.h>
#include <ovito/gui/desktop/viewport/overlays/MoveOverlayInputMode.h>
#include <ovito/gui/desktop/widgets/general/ViewportModeButton.h>
#include <ovito/gui/desktop/widgets/general/PopupUpdateComboBox.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/stdmod/viewport/ColorLegendOverlay.h>
#include <ovito/stdobj/properties/Property.h>
#include "ColorLegendOverlayEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorLegendOverlayEditor);
SET_OVITO_OBJECT_EDITOR(ColorLegendOverlay, ColorLegendOverlayEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ColorLegendOverlayEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Color legend"), rolloutParams, "manual:viewport_layers.color_legend");

    // Create the rollout contents.
    QVBoxLayout* parentLayout = new QVBoxLayout(rollout);
    parentLayout->setContentsMargins(4,4,4,4);
    parentLayout->setSpacing(4);

    QGroupBox* sourceBox = new QGroupBox(tr("Color legend source:"));
    parentLayout->addWidget(sourceBox);
    QGridLayout* sourceLayout = new QGridLayout(sourceBox);
    sourceLayout->setContentsMargins(4,4,4,4);
    sourceLayout->setColumnStretch(1, 1);
    sourceLayout->setSpacing(4);

    PipelineSelectionParameterUI* pipelineUI = createParamUI<PipelineSelectionParameterUI>(PROPERTY_FIELD(ViewportOverlay::pipeline));
    sourceLayout->addWidget(new QLabel(tr("Pipeline:")), 0, 0);
    sourceLayout->addWidget(pipelineUI->comboBox(), 0, 1);

    _colorMappingsComboBox = new PopupUpdateComboBox();
    connect(this, &PropertiesEditor::contentsChanged, this, &ColorLegendOverlayEditor::updateColorMappingsList);
    connect(_colorMappingsComboBox, &PopupUpdateComboBox::dropDownActivated, this, &ColorLegendOverlayEditor::updateColorMappingsList);
    connect(_colorMappingsComboBox, qOverload<int>(&QComboBox::activated), this, &ColorLegendOverlayEditor::colorMappingSelectedSelected);
    sourceLayout->addWidget(new QLabel(tr("Color mapping:")), 1, 0);
    sourceLayout->addWidget(_colorMappingsComboBox, 1, 1);

    QGroupBox* positionBox = new QGroupBox(tr("Positioning"));
    parentLayout->addWidget(positionBox);
    QGridLayout* positionLayout = new QGridLayout(positionBox);
    positionLayout->setContentsMargins(4,4,4,4);
    positionLayout->setColumnStretch(1, 1);
    positionLayout->setColumnStretch(2, 1);
    positionLayout->setSpacing(4);
    positionLayout->setHorizontalSpacing(4);
    int subrow = 0;

    VariantComboBoxParameterUI* alignmentPUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::alignment));
    positionLayout->addWidget(new QLabel(tr("Alignment:")), subrow, 0);
    positionLayout->addWidget(alignmentPUI->comboBox(), subrow++, 1, 1, 2);
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_left"), tr("Top left"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top"), tr("Top"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_right"), tr("Top right"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_right"), tr("Right"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_right"), tr("Bottom right"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom"), tr("Bottom"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_left"), tr("Bottom left"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_left"), tr("Left"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignLeft)));

    VariantComboBoxParameterUI* orientationPUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::orientation));
    positionLayout->addWidget(new QLabel(tr("Orientation:")), subrow, 0);
    positionLayout->addWidget(orientationPUI->comboBox(), subrow++, 1, 1, 2);
    orientationPUI->comboBox()->addItem(tr("Vertical"), QVariant::fromValue((int)Qt::Vertical));
    orientationPUI->comboBox()->addItem(tr("Horizontal"), QVariant::fromValue((int)Qt::Horizontal));

    FloatParameterUI* offsetXPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::offsetX));
    positionLayout->addWidget(new QLabel(tr("XY offset:")), subrow, 0);
    positionLayout->addLayout(offsetXPUI->createFieldLayout(), subrow, 1);
    FloatParameterUI* offsetYPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::offsetY));
    positionLayout->addLayout(offsetYPUI->createFieldLayout(), subrow++, 2);

    OORef<MoveOverlayInputMode> moveOverlayMode = OORef<MoveOverlayInputMode>::create(this);
    connect(this, &QObject::destroyed, moveOverlayMode, &ViewportInputMode::removeMode);
    ViewportModeAction* moveOverlayAction = new ViewportModeAction(ui(), tr("Move"), this, std::move(moveOverlayMode));
    moveOverlayAction->setIcon(QIcon::fromTheme("edit_mode_move"));
    moveOverlayAction->setToolTip(tr("Reposition the label in the viewport using the mouse"));
    positionLayout->addWidget(new ViewportModeButton(moveOverlayAction), subrow, 1, 1, 2, Qt::AlignRight | Qt::AlignTop);

    QGroupBox* sizeBox = new QGroupBox(tr("Size and border"));
    parentLayout->addWidget(sizeBox);
    QGridLayout* sublayout = new QGridLayout(sizeBox);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    subrow = 0;

    FloatParameterUI* sizePUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::legendSize));
    sublayout->addWidget(sizePUI->label(), subrow, 0);
    sublayout->addLayout(sizePUI->createFieldLayout(), subrow++, 1);

    _aspectRatioPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::aspectRatio));
    sublayout->addWidget(_aspectRatioPUI->label(), subrow, 0);
    sublayout->addLayout(_aspectRatioPUI->createFieldLayout(), subrow++, 1);

    _useTypeShapesPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::useTypeShapes));
    _useTypeShapesPUI->checkBox()->setText(tr("Show type shapes"));
    _useTypeShapesPUI->checkBox()->setToolTip(tr("Display the actual 3d shapes of the element types instead of flat color boxes."));
    sublayout->addWidget(_useTypeShapesPUI->checkBox(), subrow++, 1);

    _borderEnabledPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::borderEnabled));
    sublayout->addWidget(_borderEnabledPUI->checkBox(), subrow, 0);
    _borderEnabledPUI->checkBox()->setText(tr("Border:"));

    _borderColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::borderColor));
    sublayout->addWidget(_borderColorPUI->colorPicker(), subrow++, 1);

    BooleanParameterUI* backgroundEnabledPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::backgroundEnabled));
    sublayout->addWidget(backgroundEnabledPUI->checkBox(), subrow, 0);
    backgroundEnabledPUI->checkBox()->setText(tr("Background:"));

    ColorParameterUI* backgroundColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::backgroundColor));
    sublayout->addWidget(backgroundColorPUI->colorPicker(), subrow++, 1);

    QGroupBox* labelBox = new QGroupBox(tr("Text labels"));
    parentLayout->addWidget(labelBox);
    sublayout = new QGridLayout(labelBox);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 3);
    sublayout->setColumnStretch(2, 1);
    subrow = 0;

    _captionPUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::caption));
    sublayout->addWidget(new QLabel(tr("Caption:")), subrow, 0);
    sublayout->addWidget(_captionPUI->textBox(), subrow++, 1, 1, 2);

    BooleanParameterUI* titleRotationEnabledPUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::titleRotationEnabled));
    sublayout->addWidget(titleRotationEnabledPUI->checkBox(), subrow++, 1, 1, 2);
    titleRotationEnabledPUI->checkBox()->setText(tr("Rotate"));
    titleRotationEnabledPUI->setEnabled([&orientationPUI]() { return orientationPUI->comboBox()->currentIndex() == 0; }());
    connect(orientationPUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged), titleRotationEnabledPUI,
            [titleRotationEnabledPUI](int index) { titleRotationEnabledPUI->setEnabled(index == 0); });

    _label1PUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::label1));
    sublayout->addWidget(new QLabel(tr("Label 1:")), subrow, 0);
    sublayout->addWidget(_label1PUI->textBox(), subrow++, 1, 1, 2);

    _label2PUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::label2));
    sublayout->addWidget(new QLabel(tr("Label 2:")), subrow, 0);
    sublayout->addWidget(_label2PUI->textBox(), subrow++, 1, 1, 2);

    _valueFormatStringPUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::valueFormatString));
    sublayout->addWidget(new QLabel(tr("Number format:")), subrow, 0);
    sublayout->addWidget(_valueFormatStringPUI->textBox(), subrow++, 1, 1, 2);

	FloatParameterUI* fontSizePUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::fontSize));
    sublayout->addWidget(new QLabel(tr("Font size/color:")), subrow, 0);
    sublayout->addLayout(fontSizePUI->createFieldLayout(), subrow, 1);

    ColorParameterUI* textColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::textColor));
	sublayout->addWidget(textColorPUI->colorPicker(), subrow++, 2);

    BooleanParameterUI* outlineEnabledPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::outlineEnabled));
    sublayout->addWidget(outlineEnabledPUI->checkBox(), subrow, 1);

	ColorParameterUI* outlineColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::outlineColor));
	sublayout->addWidget(outlineColorPUI->colorPicker(), subrow++, 2);

    FloatParameterUI* relLabelFontSizePUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::relLabelFontSize));
    sublayout->addWidget(relLabelFontSizePUI->label(), subrow, 0);
    sublayout->addLayout(relLabelFontSizePUI->createFieldLayout(), subrow++, 1);

    FontParameterUI* labelFontPUI = createParamUI<FontParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::font));
    sublayout->addWidget(labelFontPUI->label(), subrow, 0);
    sublayout->addWidget(labelFontPUI->fontPicker(), subrow++, 1, 1, 2);

    // Tick Settings
    _tickEnabledPUI = createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::ticksEnabled));
    _tickEnabledPUI->groupBox()->setTitle(tr("Tick marks"));
    parentLayout->addWidget(_tickEnabledPUI->groupBox());

    sublayout = new QGridLayout(_tickEnabledPUI->childContainer());
    sublayout->setContentsMargins(4, 4, 4, 4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    subrow = 0;

    FloatParameterUI* tickSpacingPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorLegendOverlay::tickSpacing));
    sublayout->addWidget(tickSpacingPUI->label(), subrow, 0);
    sublayout->addLayout(tickSpacingPUI->createFieldLayout(), subrow++, 1);
    tickSpacingPUI->spinner()->setStandardValue(0.0);
    tickSpacingPUI->textBox()->setPlaceholderText(tr("‹auto›"));

    connect(this, &PropertiesEditor::contentsReplaced, this, &ColorLegendOverlayEditor::updateLabelPlaceholderTexts);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ColorLegendOverlayEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ColorLegendOverlay::AutoLabelsUpdated && source == editObject()) {
        // Update the placeholder texts of the title and label input fields whenever
        // the color legend is repainted and the automatically determined texts are recalculated.
        updateLabelPlaceholderTexts();
    }

    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Updates the combo-box list showing the available color mappings.
******************************************************************************/
void ColorLegendOverlayEditor::updateColorMappingsList()
{
    _label1PUI->setEnabled(false);
    _label2PUI->setEnabled(false);
    _valueFormatStringPUI->setEnabled(false);
    _tickEnabledPUI->setEnabled(false);
    // The type shapes can only be displayed for the element types of a typed property.
    _useTypeShapesPUI->setEnabled(false);
    _aspectRatioPUI->setEnabled(true);
    _borderEnabledPUI->setEnabled(true);
    _borderColorPUI->setEnabled(true);
    _sourceTypesHaveShapes = false;

    _colorMappingsComboBox->clear();
    if(ColorLegendOverlay* overlay = static_object_cast<ColorLegendOverlay>(editObject())) {
        // List all typed properties, PropertyColorMappings, and ColorCodingModifiers in the selected pipeline.
        // To find them, iterate over the pipeline's modifier nodes and visit all visual elements and data objects in the pipeline's output data collection.
        if(Pipeline* pipeline = overlay->pipeline()) {

            // Go through the visual elements of the pipeline and look if any one has a PropertyColorMapping attached to it.
            for(DataVis* vis : pipeline->visElements()) {
                if(vis->isEnabled()) {
                    for(const PropertyFieldDescriptor* field : vis->getOOMetaClass().propertyFields()) {
                        if(field->isReferenceField() && field->targetClass()->isDerivedFrom(PropertyColorMapping::OOClass()) && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM) && !field->isVector()) {
                            if(OORef<PropertyColorMapping> mapping = static_object_cast<PropertyColorMapping>(vis->getReferenceFieldTarget(field))) {
                                if(mapping->sourceProperty()) {
                                    // Prepend property color mappings to the front of the list.
                                    _colorMappingsComboBox->insertItem(0, QStringLiteral("%1: %2").arg(vis->objectTitle()).arg(mapping->sourceProperty().nameWithComponent()), QVariant::fromValue(mapping));
                                }
                            }
                            break;
                        }
                    }
                }
            }

            // Walk along the pipeline to find modification nodes associated with a ColorCodingModifier:
            PipelineNode* node = pipeline->head();
            while(node) {
                if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(node)) {
                    if(OORef<ColorCodingModifier> mod = dynamic_object_cast<ColorCodingModifier>(modNode->modifier())) {
                        // Prepend color coding modifiers to the front of the list.
                        _colorMappingsComboBox->insertItem(0, tr("Color coding: %1").arg(mod->sourceProperty().nameWithComponent()), QVariant::fromValue(mod));
                    }
                    node = modNode->input();
                }
                else break;
            }

            // Pipeline evaluations require a valid execution context.
            handleExceptions([&] {

                // Now evaluate the pipeline and look for typed properties in its output data collection.
                const PipelineFlowState& state = pipeline->getCachedPipelineOutput(currentAnimationTime());
                for(const ConstDataObjectPath& dataPath : ColorLegendOverlay::listTypedProperties(state)) {
                    QVariant ref = QVariant::fromValue(PropertyDataObjectReference(dataPath));

                    // Append typed properties at the end of the list.
                    if(_colorMappingsComboBox->findData(ref) < 0)
                        _colorMappingsComboBox->addItem(dataPath.toUIString(), std::move(ref));
                }

                // Determine whether the element types of the selected source property can be represented by 3d shapes.
                // Only particle types provide such shapes; generic element types, e.g. structure types, do not.
                if(overlay->sourceProperty()) {
                    const ConstDataObjectPath propertyPath = state.getObject(overlay->sourceProperty());
                    if(const Property* property = propertyPath.lastAs<Property>()) {
                        if(property->isTypedProperty()) {
                            const DataVis* containerVis = (propertyPath.size() >= 2) ? propertyPath[propertyPath.size() - 2]->visElement() : nullptr;
                            _sourceTypesHaveShapes = !ColorLegendOverlay::getTypeSymbols(property, containerVis).empty();
                        }
                    }
                }
            });
        }

        // Select the item in the list that corresponds to the current parameter value.
        if(overlay->modifier()) {
            int index = _colorMappingsComboBox->findData(QVariant::fromValue<OORef<ColorCodingModifier>>(overlay->modifier()));
            if(index >= 0)
                _colorMappingsComboBox->setCurrentIndex(index);
            else {
                _colorMappingsComboBox->addItem(QIcon(":/guibase/mainwin/status/status_warning.svg"), overlay->modifier()->objectTitle());
                _colorMappingsComboBox->setCurrentIndex(_colorMappingsComboBox->count() - 1);
            }
            _label1PUI->setEnabled(true);
            _label2PUI->setEnabled(true);
            _valueFormatStringPUI->setEnabled(true);
            _tickEnabledPUI->setEnabled(true);
        }
        else if(overlay->colorMapping()) {
            int index = _colorMappingsComboBox->findData(QVariant::fromValue<OORef<PropertyColorMapping>>(overlay->colorMapping()));
            if(index >= 0)
                _colorMappingsComboBox->setCurrentIndex(index);
            else {
                _colorMappingsComboBox->addItem(QIcon(":/guibase/mainwin/status/status_warning.svg"), overlay->colorMapping()->sourceProperty().nameWithComponent());
                _colorMappingsComboBox->setCurrentIndex(_colorMappingsComboBox->count() - 1);
            }
            _label1PUI->setEnabled(true);
            _label2PUI->setEnabled(true);
            _valueFormatStringPUI->setEnabled(true);
            _tickEnabledPUI->setEnabled(true);
        }
        else if(overlay->sourceProperty()) {
            int index = _colorMappingsComboBox->findData(QVariant::fromValue(overlay->sourceProperty()));
            if(index >= 0)
                _colorMappingsComboBox->setCurrentIndex(index);
            else {
                _colorMappingsComboBox->addItem(QIcon(":/guibase/mainwin/status/status_warning.svg"), overlay->sourceProperty().dataTitleOrPath());
                _colorMappingsComboBox->setCurrentIndex(_colorMappingsComboBox->count() - 1);
            }
            _useTypeShapesPUI->setEnabled(true);
            // Neither the aspect ratio nor the border apply if the types are really rendered as 3d shapes.
            // The legend cells are then forced to be square, and the symbols stand on a transparent background,
            // which a border would turn into an opaque block. For types without a shape the legend falls back
            // to flat color boxes, and both parameters remain in effect.
            const bool typeShapesActive = overlay->useTypeShapes() && _sourceTypesHaveShapes;
            _aspectRatioPUI->setEnabled(!typeShapesActive);
            _borderEnabledPUI->setEnabled(!typeShapesActive);
            _borderColorPUI->setEnabled(!typeShapesActive);
        }
        else {
            _colorMappingsComboBox->addItem(QIcon(":/guibase/mainwin/status/status_warning.svg"), tr("<none>"));
            _colorMappingsComboBox->setCurrentIndex(_colorMappingsComboBox->count() - 1);
        }
    }
    if(_colorMappingsComboBox->count() == 0)
        _colorMappingsComboBox->addItem(QIcon(":/guibase/mainwin/status/status_warning.svg"), tr("<none>"));
}

/******************************************************************************
* Is called when the user selects a new source color mapping for the color legend.
******************************************************************************/
void ColorLegendOverlayEditor::colorMappingSelectedSelected()
{
    if(ColorLegendOverlay* overlay = static_object_cast<ColorLegendOverlay>(editObject())) {
        performTransaction(tr("Select legend color mapping"), [&]() {
            QVariant selectedData = static_cast<QComboBox*>(sender())->currentData();

            if(selectedData.canConvert<OORef<ColorCodingModifier>>()) {
                overlay->setModifier(selectedData.value<OORef<ColorCodingModifier>>());
                overlay->setColorMapping(nullptr);
                overlay->setSourceProperty({});
            }
            else if(selectedData.canConvert<OORef<PropertyColorMapping>>()) {
                overlay->setColorMapping(selectedData.value<OORef<PropertyColorMapping>>());
                overlay->setModifier(nullptr);
                overlay->setSourceProperty({});
            }
            else if(selectedData.canConvert<PropertyDataObjectReference>()) {
                overlay->setModifier(nullptr);
                overlay->setColorMapping(nullptr);
                overlay->setSourceProperty(selectedData.value<PropertyDataObjectReference>());
            }
        });
    }
}

/******************************************************************************
* Updates the placeholder texts of the label input fields to reflect the current values.
******************************************************************************/
void ColorLegendOverlayEditor::updateLabelPlaceholderTexts()
{
    QString placeholderTitle;
    QString placeholderLabel1;
    QString placeholderLabel2;

    if(ColorLegendOverlay* overlay = static_object_cast<ColorLegendOverlay>(editObject())) {
        if(!overlay->_autoTitleText.isEmpty())
            placeholderTitle = QStringLiteral("‹%1›").arg(overlay->_autoTitleText);
        if(!overlay->_autoLabel1Text.isEmpty())
            placeholderLabel1 = QStringLiteral("‹%1›").arg(overlay->_autoLabel1Text);
        if(!overlay->_autoLabel2Text.isEmpty())
            placeholderLabel2 = QStringLiteral("‹%1›").arg(overlay->_autoLabel2Text);
    }

    _captionPUI->lineEdit()->setPlaceholderText(placeholderTitle);
    _label1PUI->lineEdit()->setPlaceholderText(placeholderLabel1);
    _label2PUI->lineEdit()->setPlaceholderText(placeholderLabel2);
}

}   // End of namespace
