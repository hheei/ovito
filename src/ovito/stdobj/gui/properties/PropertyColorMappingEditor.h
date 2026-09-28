// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>

namespace Ovito {

/**
 * A properties editor for the PropertyColorMapping class.
 */
class OVITO_STDOBJGUI_EXPORT PropertyColorMappingEditor : public PropertiesEditor
{
    OVITO_CLASS(PropertyColorMappingEditor)

public:

    /// Sets the property container(s) containing the input properties the user can choose from.
    void setPropertyContainers(std::vector<DataOORef<const PropertyContainer>> containers);

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Returns an icon representing the given color map class.
    QIcon iconFromColorMapClass(OvitoClassPtr clazz);

    /// Returns an icon representing the given color map.
    QIcon iconFromColorMap(ColorCodingGradient* map);

    /// Determines the min/max range of values in the selected input property.
    std::optional<std::pair<FloatType, FloatType>> determineValueRange() const;

    /// Determine the property value corresponding to the given relative position in the range interval.
    FloatType computeRangeValue(FloatType t) const;

protected Q_SLOTS:

    /// Updates the display for the color gradient.
    void updateColorGradient();

    /// Is called when the user selects a color gradient in the list box.
    void onColorGradientSelected(int index);

    /// Is called when the user presses the "Adjust range" button.
    void onAdjustRange();

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

    /// Cache for the color gradient preview images
    QHash<std::pair<int, int>, QImage> _colorGradientCache;

    /// Indicates the combo box already contains an item for a custom color map.
    bool _gradientListContainCustomItem;

    /// Label that displays the color gradient picture.
    QLabel* _colorLegendLabel;

    PropertyReferenceParameterUI* _sourcePropertyUI;
    FloatParameterUI* _startValueUI;
    FloatParameterUI* _endValueUI;
    QPushButton* _adjustRangeBtn;
    QPushButton* _reverseRangeBtn;
};

}   // End of namespace
