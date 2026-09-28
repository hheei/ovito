// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TCBInterpolationControllers.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FloatTCBAnimationKey);
OVITO_CLASSINFO(FloatTCBAnimationKey, "ClassNameAlias", "TCBAnimationKey<FloatAnimationKey>");  // For backward compatibility with OVITO 3.10
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, easeTo);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, easeFrom);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, tension);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, continuity);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, bias);
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, easeTo, "Ease to");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, easeFrom, "Ease from");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, tension, "Tension");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, continuity, "Continuity");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, bias, "Bias");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FloatTCBAnimationKey, easeTo, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FloatTCBAnimationKey, easeFrom, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FloatTCBAnimationKey, tension, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FloatTCBAnimationKey, continuity, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FloatTCBAnimationKey, bias, FloatParameterUnit, -1, 1);

IMPLEMENT_CREATABLE_OVITO_CLASS(PositionTCBAnimationKey);
OVITO_CLASSINFO(PositionTCBAnimationKey, "ClassNameAlias", "TCBAnimationKey<PositionAnimationKey>");  // For backward compatibility with OVITO 3.10
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, easeTo);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, easeFrom);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, tension);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, continuity);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, bias);
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, easeTo, "Ease to");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, easeFrom, "Ease from");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, tension, "Tension");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, continuity, "Continuity");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, bias, "Bias");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PositionTCBAnimationKey, easeTo, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PositionTCBAnimationKey, easeFrom, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(PositionTCBAnimationKey, tension, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(PositionTCBAnimationKey, continuity, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(PositionTCBAnimationKey, bias, FloatParameterUnit, -1, 1);

IMPLEMENT_CREATABLE_OVITO_CLASS(TCBPositionController);

}   // End of namespace
