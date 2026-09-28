// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "SplineInterpolationControllers.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FloatSplineAnimationKey);
OVITO_CLASSINFO(FloatSplineAnimationKey, "ClassNameAlias", "SplineAnimationKey<FloatAnimationKey>");  // For backward compatibility with OVITO 3.10
DEFINE_PROPERTY_FIELD(FloatSplineAnimationKey, inTangent);
DEFINE_PROPERTY_FIELD(FloatSplineAnimationKey, outTangent);
SET_PROPERTY_FIELD_LABEL(FloatSplineAnimationKey, inTangent, "In Tangent");
SET_PROPERTY_FIELD_LABEL(FloatSplineAnimationKey, outTangent, "Out Tangent");

IMPLEMENT_CREATABLE_OVITO_CLASS(PositionSplineAnimationKey);
OVITO_CLASSINFO(PositionSplineAnimationKey, "ClassNameAlias", "SplineAnimationKey<PositionAnimationKey>");  // For backward compatibility with OVITO 3.10
DEFINE_PROPERTY_FIELD(PositionSplineAnimationKey, inTangent);
DEFINE_PROPERTY_FIELD(PositionSplineAnimationKey, outTangent);
SET_PROPERTY_FIELD_LABEL(PositionSplineAnimationKey, inTangent, "In Tangent");
SET_PROPERTY_FIELD_LABEL(PositionSplineAnimationKey, outTangent, "Out Tangent");

IMPLEMENT_CREATABLE_OVITO_CLASS(SplinePositionController);

}   // End of namespace
