// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "ConstantControllers.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ConstFloatController);
IMPLEMENT_CREATABLE_OVITO_CLASS(ConstIntegerController);
IMPLEMENT_CREATABLE_OVITO_CLASS(ConstVectorController);
IMPLEMENT_CREATABLE_OVITO_CLASS(ConstPositionController);
IMPLEMENT_CREATABLE_OVITO_CLASS(ConstRotationController);
IMPLEMENT_CREATABLE_OVITO_CLASS(ConstScalingController);
DEFINE_PROPERTY_FIELD(ConstFloatController, value);
DEFINE_PROPERTY_FIELD(ConstIntegerController, value);
DEFINE_PROPERTY_FIELD(ConstVectorController, value);
DEFINE_PROPERTY_FIELD(ConstPositionController, value);
DEFINE_PROPERTY_FIELD(ConstRotationController, value);
DEFINE_PROPERTY_FIELD(ConstScalingController, value);

}   // End of namespace
