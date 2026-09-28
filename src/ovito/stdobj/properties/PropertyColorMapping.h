// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/core/rendering/PseudoColorMapping.h>
#include <ovito/core/rendering/ColorCodingGradient.h>

namespace Ovito {

/**
 * \brief A transfer function that maps property values to display colors.
 */
class OVITO_STDOBJ_EXPORT PropertyColorMapping : public RefTarget
{
    OVITO_CLASS(PropertyColorMapping)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Creates a PseudoColorMapping that can be used for rendering of graphics primitives.
    PseudoColorMapping pseudoColorMapping() const;

    /// Determines the min/max range of values stored in the given property array.
    std::optional<std::pair<FloatType, FloatType>> determineValueRange(const Property* pseudoColorProperty, int pseudoColorPropertyComponent) const;

    /// Swaps the minimum and maximum values to reverse the color scale.
    void reverseRange();

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// This object converts a scalar values to an RGB color.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<ColorCodingGradient>, colorGradient, setColorGradient);

    /// This lower bound of the input value internal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, startValue, setStartValue);

    /// This upper bound of the input value internal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, endValue, setEndValue);

    /// The input property (including an optional vector component) that is used as data source for the coloring.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty, setSourceProperty);

    /// Controls whether the value range of the color map is automatically symmetrized (centered) around 0.
    /// This is intended to be used with diverging color maps like blue-white-red.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, symmetricRange, setSymmetricRange);

    /// Use a discrete version of the color map with one color value per integer value in the value range.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useDiscreteColorMap, setUseDiscreteColorMap);
};

}   // End of namespace
