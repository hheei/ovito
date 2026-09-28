// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Scene);
OVITO_CLASSINFO(Scene, "ClassNameAlias", "RootSceneNode"); // For backward compatibility with OVITO 3.7.11.
DEFINE_REFERENCE_FIELD(Scene, animationSettings);
DEFINE_REFERENCE_FIELD(Scene, selection);
DEFINE_PROPERTY_FIELD(Scene, orbitCenterMode);
DEFINE_PROPERTY_FIELD(Scene, userOrbitCenter);
SET_PROPERTY_FIELD_LABEL(Scene, animationSettings, "Animation Settings");
SET_PROPERTY_FIELD_LABEL(Scene, selection, "Selection");

/******************************************************************************
* Constructor.
******************************************************************************/
void Scene::initializeObject(ObjectInitializationFlags flags, AnimationSettings* animationSettings)
{
    SceneNode::initializeObject(flags);

    setAnimationSettings(animationSettings);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setSceneNodeName("Scene");

        // Create child objects for animation settings and node selection set.
        if(!this->animationSettings())
            setAnimationSettings(OORef<AnimationSettings>::create(flags));
        setSelection(OORef<SelectionSet>::create(flags));
    }
}

/******************************************************************************
* Searches the scene for a node with the given name.
******************************************************************************/
SceneNode* Scene::getNodeByName(const QString& nodeName) const
{
    SceneNode* result = nullptr;
    visitChildren([nodeName, &result](SceneNode* node) -> bool {
        if(node->sceneNodeName() == nodeName) {
            result = node;
            return false;
        }
        return true;
    });
    return result;
}

/******************************************************************************
* Generates a name for a node that is unique throughout the scene.
******************************************************************************/
QString Scene::makeNameUnique(QString baseName) const
{
    // Remove any existing digits from end of base name.
    if(baseName.size() > 2 &&
        baseName.at(baseName.size()-1).isDigit() && baseName.at(baseName.size()-2).isDigit())
        baseName.chop(2);

    // Keep appending different numbers until we arrive at a unique name.
    for(int i = 1; ; i++) {
        QString newName = baseName + QString::number(i).rightJustified(2, '0');
        if(getNodeByName(newName) == nullptr)
            return newName;
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool Scene::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::RequestGoToAnimationTime) {
        int frame = static_cast<const RequestGoToAnimationTimeEvent&>(event).time().frame();
        if(animationSettings() && frame >= animationSettings()->firstFrame() && frame <= animationSettings()->lastFrame())
            animationSettings()->setCurrentFrame(frame);
    }

    return SceneNode::referenceEvent(source, event);
}

/******************************************************************************
* Is called whenever one of the child nodes in the tree has generated a AnimationFramesChanged event.
******************************************************************************/
void Scene::onAnimationFramesChanged()
{
    if(!shouldIgnoreChanges()) {
        // Automatically adjust scene's animation interval to length of loaded source animations.
        if(animationSettings() && animationSettings()->autoAdjustInterval()) {
            UndoSuspender noUndo;
            animationSettings()->updateAnimationFrameLabels();
            animationSettings()->adjustAnimationInterval();
        }
    }
}

}   // End of namespace
