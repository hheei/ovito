// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief The root node of a scene tree made of SceneNode instances.
 */
class OVITO_CORE_EXPORT Scene : public SceneNode
{
    OVITO_CLASS(Scene)

public:

    enum OrbitCenterMode {
        ORBIT_SELECTION_CENTER,     ///< Take the center of mass of the current selection as orbit center.
                                    ///< If there is no selection, use scene bounding box.
        ORBIT_USER_DEFINED          ///< Use the orbit center set by the user.
    };
    Q_ENUM(OrbitCenterMode);

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, AnimationSettings* animationSettings = nullptr);

    /// Searches the scene for a node with the given name.
    SceneNode* getNodeByName(const QString& nodeName) const;

    /// Generates a name for a node that is unique throughout the scene.
    QString makeNameUnique(QString baseName) const;

    /// Returns whether this is the root scene node.
    virtual bool isRootNode() const override { return true; }

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called whenever one of the child nodes in the tree has generated a AnimationFramesChanged event.
    virtual void onAnimationFramesChanged() override;

    /// Computes the scene node's local bounding box.
    virtual Box3 localBoundingBoxInternal(AnimationTime time, TimeInterval& validity) const override { return Box3(); }

private:

    /// The animation timeline of this scene.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<AnimationSettings>, animationSettings, setAnimationSettings);

    /// The current object selection set.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SelectionSet>, selection, setSelection, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_DEEP_COPY | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

    /// Controls around which point the viewport camera should orbit.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(OrbitCenterMode{ORBIT_SELECTION_CENTER}, orbitCenterMode, setOrbitCenterMode, PROPERTY_FIELD_NO_UNDO);

    /// Position of the orbiting center picked by the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Point3{Point3::Origin()}, userOrbitCenter, setUserOrbitCenter, PROPERTY_FIELD_NO_UNDO);
};

}   // End of namespace
