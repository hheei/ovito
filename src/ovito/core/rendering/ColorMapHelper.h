// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/ColorCodingGradient.h>

namespace Ovito {
namespace DiscreteColorMap {

/// Computes the number of different levels in a discrete colormap such that
/// each integer value in the range [startValue, endValue] is mapped to a unique color.
template<typename T>
inline int binCount(T startValue, T endValue)
{
    // Protect against overflow
    return (int)std::min(std::round(std::abs(endValue - startValue)), (T)255) + 1;
}

/// Maps the color value t [0,1] to its discrete value based on the discrete colormap bin count.
template<typename T, typename V>
inline T mapValue(T t, V binCount)
{
    static_assert(std::is_floating_point_v<T>, "T must be a floating point type.");
    static_assert(std::is_integral_v<V>, "V must be an integral type.");

    if(binCount <= 1) {
        return (T)0.5;
    }
    if(t >= (T)1) {
        return (T)1;
    }
    if(t <= 0) {
        return (T)0;
    }
    const T binSize = (T)1 / (T)(binCount - 1);
    const T binIndex = std::trunc(t * (T)binCount);

    return binIndex * binSize;
}

}  // namespace DiscreteColorMap

namespace ColorMap {

/// Generates a color gradient image for the given color map.
/// binCount <= 0 indicates that the color gradient is not a discrete colormap.
template<int legendHeight>
QImage generateImage(const ColorCodingGradient* gradient, int binCount = -1)
{
    QImage image{1, legendHeight, QImage::Format_RGB32};
    for(int y = 0; y < legendHeight; y++) {
        FloatType t = (FloatType)y / (legendHeight - 1);
        // binCount <= 0 indicates that the color gradient is not a discrete colormap.
        t = (binCount <= 0) ? t : DiscreteColorMap::mapValue(t, binCount);
        const Color color = gradient->valueToColor(1.0 - t);
        image.setPixel(0, y, QColor(color).rgb());
    }
    return image;
}

}  // namespace ColorMap

}  // namespace Ovito