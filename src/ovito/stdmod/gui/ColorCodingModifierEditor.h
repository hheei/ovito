// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdmod/modifiers/ColorCodingModifier.h>

namespace Ovito {

/**
 * A properties editor for the ColorCodingModifier class.
 */
class ColorCodingModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ColorCodingModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Returns an icon representing the given color map class.
    QIcon iconFromColorMapClass(OvitoClassPtr clazz);

    /// Returns an icon representing the given color map.
    QIcon iconFromColorMap(ColorCodingGradient* map);

    /// Determine the property value corresponding to the given relative position in the range interval.
    FloatType computeRangeValue(FloatType t) const;

protected Q_SLOTS:

    /// This is called whenever the parameters of the ColorCodingModifier change.
    void onModifierChanged();

    /// Is called whenever the modifier has been newly evaluated and has auto-adjusted the value range.
    void autoRangeChanged();

    /// Updates the display for the color gradient.
    void updateColorGradient();

    /// Is called when the user selects a color gradient in the list box.
    void onColorGradientSelected(int index);

    /// Is called when the user presses the "Adjust range" button.
    void onAdjustRange();

    /// Is called when the user presses the "Adjust range over all frames" button.
    void onAdjustRangeGlobal();

    /// Is called when the user presses the "Reverse range" button.
    void onReverseRange();

    /// Is called when the user presses the "Export color scale" button.
    void onExportColorScale();

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The list of available color gradients.
    QComboBox* _colorGradientList;

    /// Indicates the combo box already contains an item for a custom color map.
    bool _gradientListContainCustomItem;

    /// Label that displays the color gradient picture.
    QLabel* _colorLegendLabel;

    /// Cache for the color gradient preview images
    QHash<std::pair<int, int>, QImage> _colorGradientCache;

    PropertyReferenceParameterUI* _sourcePropertyUI;
    FloatParameterUI* _startValueUI;
    FloatParameterUI* _endValueUI;
    QPushButton* _adjustRangeBtn;
    QPushButton* _adjustRangeGlobalBtn;
    QPushButton* _reverseRangeBtn;
    FloatType _lastAutoRangeMinValue;
    FloatType _lastAutoRangeMaxValue;
};

}   // End of namespace
