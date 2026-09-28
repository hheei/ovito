// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

/**
 * \brief A transfer function for the opacity used in volume rendering.
 */
class OVITO_CORE_EXPORT OpacityFunction : public DataObject
{
    OVITO_CLASS(OpacityFunction)

public:

    static constexpr size_t DEFAULT_TABULATION_SIZE = 256;

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the optimal number of samples for tabulating the opacity function.
    size_t optimalTabulationSize() const {
        return table().size();
    }

    /// Produces a tabulated representation of the opacity function.
    void tabulateOpacityValues(std::span<float> buffer) const;

    /// Implements a free-hand drawing operation on the opacity function.
    void freeDraw(std::span<const Point2> drawPath);

    /// Restores the default opacity function state.
    void reset();

private:

    /// The tabulated opacity function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(std::vector<FloatType>{}, table, setTable);
};

}   // End of namespace
