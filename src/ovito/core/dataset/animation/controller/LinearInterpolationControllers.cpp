// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "LinearInterpolationControllers.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinearFloatController);
IMPLEMENT_CREATABLE_OVITO_CLASS(LinearIntegerController);
IMPLEMENT_CREATABLE_OVITO_CLASS(LinearVectorController);
IMPLEMENT_CREATABLE_OVITO_CLASS(LinearPositionController);
IMPLEMENT_CREATABLE_OVITO_CLASS(LinearRotationController);
IMPLEMENT_CREATABLE_OVITO_CLASS(LinearScalingController);

}   // End of namespace
