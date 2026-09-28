// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include "AnimationKeys.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(AnimationKey);
IMPLEMENT_CREATABLE_OVITO_CLASS(FloatAnimationKey);
IMPLEMENT_CREATABLE_OVITO_CLASS(IntegerAnimationKey);
IMPLEMENT_CREATABLE_OVITO_CLASS(Vector3AnimationKey);
IMPLEMENT_CREATABLE_OVITO_CLASS(PositionAnimationKey);
IMPLEMENT_CREATABLE_OVITO_CLASS(RotationAnimationKey);
IMPLEMENT_CREATABLE_OVITO_CLASS(ScalingAnimationKey);
DEFINE_PROPERTY_FIELD(AnimationKey, time);
DEFINE_PROPERTY_FIELD(FloatAnimationKey, value);
DEFINE_PROPERTY_FIELD(IntegerAnimationKey, value);
DEFINE_PROPERTY_FIELD(Vector3AnimationKey, value);
DEFINE_PROPERTY_FIELD(PositionAnimationKey, value);
DEFINE_PROPERTY_FIELD(RotationAnimationKey, value);
DEFINE_PROPERTY_FIELD(ScalingAnimationKey, value);
SET_PROPERTY_FIELD_LABEL(AnimationKey, time, "Time");
SET_PROPERTY_FIELD_UNITS(AnimationKey, time, TimeParameterUnit);
SET_PROPERTY_FIELD_LABEL(FloatAnimationKey, value, "Value");
SET_PROPERTY_FIELD_LABEL(IntegerAnimationKey, value, "Value");
SET_PROPERTY_FIELD_LABEL(Vector3AnimationKey, value, "Value");
SET_PROPERTY_FIELD_LABEL(PositionAnimationKey, value, "Value");
SET_PROPERTY_FIELD_LABEL(RotationAnimationKey, value, "Value");
SET_PROPERTY_FIELD_LABEL(ScalingAnimationKey, value, "Value");

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void AnimationKey::loadFromStreamComplete(ObjectLoadStream& stream)
{
    RefTarget::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.7:
    // Convert legacy time value. This requires access to the AnimationSettings object, which is stored in the scene.
    if(stream.formatVersion() <= 30008) {
        if(DataSet* dataset = stream.datasetBeingLoaded()) {
            if(Viewport* vp = dataset->viewportConfig()->activeViewport()) {
                if(Scene* scene = vp->scene()) {
                    if(scene->animationSettings()) {
                        int ticksPerFrame = (int)std::round(4800.0f / scene->animationSettings()->framesPerSecond());
                        setTime(AnimationTime::fromFrame(time().ticks() / ticksPerFrame));
                    }
                }
            }
        }
    }
}

}   // End of namespace
