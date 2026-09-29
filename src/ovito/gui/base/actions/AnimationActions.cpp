// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/******************************************************************************
* Handles the ACTION_GOTO_START_OF_ANIMATION command.
******************************************************************************/
void ActionManager::on_AnimationGotoStart_triggered()
{
    if(AnimationSettings* anim = activeAnimationSettings())
        anim->jumpToAnimationStart();
}

/******************************************************************************
* Handles the ACTION_GOTO_END_OF_ANIMATION command.
******************************************************************************/
void ActionManager::on_AnimationGotoEnd_triggered()
{
    if(AnimationSettings* anim = activeAnimationSettings())
        anim->jumpToAnimationEnd();
}

/******************************************************************************
* Handles the ACTION_GOTO_PREVIOUS_FRAME command.
******************************************************************************/
void ActionManager::on_AnimationGotoPreviousFrame_triggered()
{
    if(AnimationSettings* anim = activeAnimationSettings())
        anim->jumpToPreviousFrame();
}

/******************************************************************************
* Handles the ACTION_GOTO_NEXT_FRAME command.
******************************************************************************/
void ActionManager::on_AnimationGotoNextFrame_triggered()
{
    if(AnimationSettings* anim = activeAnimationSettings())
        anim->jumpToNextFrame();
}

/******************************************************************************
* Handles the ACTION_START_ANIMATION_PLAYBACK command.
******************************************************************************/
void ActionManager::on_AnimationStartPlayback_triggered()
{
    if(!getCommand(ACTION_TOGGLE_ANIMATION_PLAYBACK)->isChecked())
        getCommand(ACTION_TOGGLE_ANIMATION_PLAYBACK)->trigger();
}

/******************************************************************************
* Handles the ACTION_STOP_ANIMATION_PLAYBACK command.
******************************************************************************/
void ActionManager::on_AnimationStopPlayback_triggered()
{
    if(getCommand(ACTION_TOGGLE_ANIMATION_PLAYBACK)->isChecked())
        getCommand(ACTION_TOGGLE_ANIMATION_PLAYBACK)->trigger();
}

}   // End of namespace
