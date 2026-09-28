// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/controller/PRSTransformationController.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/linalg/AffineDecomposition.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(PRSTransformationController);
DEFINE_REFERENCE_FIELD(PRSTransformationController, positionController);
DEFINE_REFERENCE_FIELD(PRSTransformationController, rotationController);
DEFINE_REFERENCE_FIELD(PRSTransformationController, scalingController);
SET_PROPERTY_FIELD_LABEL(PRSTransformationController, positionController, "Position");
SET_PROPERTY_FIELD_LABEL(PRSTransformationController, rotationController, "Rotation");
SET_PROPERTY_FIELD_LABEL(PRSTransformationController, scalingController, "Scaling");
SET_PROPERTY_FIELD_UNITS(PRSTransformationController, positionController, WorldParameterUnit);
SET_PROPERTY_FIELD_UNITS(PRSTransformationController, rotationController, AngleParameterUnit);
SET_PROPERTY_FIELD_UNITS(PRSTransformationController, scalingController, PercentParameterUnit);

/******************************************************************************
* Constructor.
******************************************************************************/
void PRSTransformationController::initializeObject(ObjectInitializationFlags flags)
{
    Controller::initializeObject(flags);

    if(!flags.testFlag(DontInitializeObject)) {
        // Create sub-controllers.
        setPositionController(ControllerManager::createPositionController());
        setRotationController(ControllerManager::createRotationController());
        setScalingController(ControllerManager::createScalingController());
    }
}

/******************************************************************************
* Let the controller apply its value at a certain time to the input value.
******************************************************************************/
void PRSTransformationController::applyTransformation(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval)
{
    positionController()->applyTranslation(time, result, validityInterval);
    rotationController()->applyRotation(time, result, validityInterval);
    scalingController()->applyScaling(time, result, validityInterval);
}

/******************************************************************************
* Sets the controller's value at the specified time.
******************************************************************************/
void PRSTransformationController::setTransformationValue(AnimationTime time, const AffineTransformation& newValue, bool isAbsolute)
{
    AffineDecomposition decomp(newValue);
    positionController()->setPositionValue(time, decomp.translation, isAbsolute);
    rotationController()->setRotationValue(time, Rotation(decomp.rotation), isAbsolute);
    scalingController()->setScalingValue(time, decomp.scaling, isAbsolute);
}

/******************************************************************************
* Adjusts the controller's value after a scene node has gotten a new parent node.
******************************************************************************/
void PRSTransformationController::changeParent(AnimationTime time, const AffineTransformation& oldParentTM, const AffineTransformation& newParentTM, SceneNode* contextNode)
{
    positionController()->changeParent(time, oldParentTM, newParentTM, contextNode);
    rotationController()->changeParent(time, oldParentTM, newParentTM, contextNode);
    scalingController()->changeParent(time, oldParentTM, newParentTM, contextNode);
}

/******************************************************************************
* Computes the largest time interval containing the given time during which the
* controller's value is constant.
******************************************************************************/
TimeInterval PRSTransformationController::validityInterval(AnimationTime time)
{
    TimeInterval iv = TimeInterval::infinite();
    iv.intersect(positionController()->validityInterval(time));
    iv.intersect(rotationController()->validityInterval(time));
    iv.intersect(scalingController()->validityInterval(time));
    return iv;
}

}   // End of namespace
