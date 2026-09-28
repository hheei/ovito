// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/controller/ConstantControllers.h>
#include <ovito/core/dataset/animation/controller/LinearInterpolationControllers.h>
#include <ovito/core/dataset/animation/controller/SplineInterpolationControllers.h>
#include <ovito/core/dataset/animation/controller/TCBInterpolationControllers.h>
#include <ovito/core/dataset/animation/controller/PRSTransformationController.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(Controller);

/******************************************************************************
* Creates a new float controller.
******************************************************************************/
OORef<Controller> ControllerManager::createFloatController()
{
    return OORef<LinearFloatController>::create();
}

/******************************************************************************
* Creates a new integer controller.
******************************************************************************/
OORef<Controller> ControllerManager::createIntController()
{
    return OORef<LinearIntegerController>::create();
}

/******************************************************************************
* Creates a new Vector3 controller.
******************************************************************************/
OORef<Controller> ControllerManager::createVector3Controller()
{
    return OORef<LinearVectorController>::create();
}

/******************************************************************************
* Creates a new position controller.
******************************************************************************/
OORef<Controller> ControllerManager::createPositionController()
{
    return OORef<SplinePositionController>::create();
}

/******************************************************************************
* Creates a new rotation controller.
******************************************************************************/
OORef<Controller> ControllerManager::createRotationController()
{
    return OORef<LinearRotationController>::create();
}

/******************************************************************************
* Creates a new scaling controller.
******************************************************************************/
OORef<Controller> ControllerManager::createScalingController()
{
    return OORef<LinearScalingController>::create();
}

/******************************************************************************
* Creates a new transformation controller.
******************************************************************************/
OORef<Controller> ControllerManager::createTransformationController()
{
    return OORef<PRSTransformationController>::create();
}

/******************************************************************************
* Queries whether the user has activated auto-key mode and controllers should automatically
* generate new animation keys whenever their current value is changed by the user.
******************************************************************************/
bool ControllerManager::isAutoGenerateAnimationKeysEnabled()
{
    if(Task* task = this_task::get()) {
        OVITO_ASSERT(task->userInterface());
        return task->userInterface()->isAutoGenerateAnimationKeysEnabled();
    }
    return false;
}

}   // End of namespace
