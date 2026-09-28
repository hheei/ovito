// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file LookAtController.h
 * \brief Contains the definition of the Ovito::LookAtController class.
 */

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/animation/controller/Controller.h>

namespace Ovito {

/**
 * \brief Rotation controller that lets an object always "look" at another scene node.
 *
 * This RotationController computes a rotation matrix for a SceneNode such
 * that it always faces into the direction of the target SceneNode.
 */
class OVITO_CORE_EXPORT LookAtController : public Controller
{
public:
    /// Give this class its own metaclass.
    class OVITO_CORE_EXPORT LookAtControllerClass : public RefTarget::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using Controller::OOMetaClass::OOMetaClass;

        /// Provides a custom function that takes care of the deserialization of a serialized property field.
        /// This is needed for backward compatibility with OVITO 3.11.
        virtual SerializedPropertyField::CustomDeserializationFunctionPtr overrideFieldDeserialization(LoadStream& stream, const SerializedPropertyField& field) const override;
    };
    OVITO_CLASS_META(LookAtController, LookAtControllerClass)

public:

    /// \brief Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypeRotation; }

    /// Queries the controller for its value at a certain time.
    virtual void getRotationValue(AnimationTime time, Rotation& result, TimeInterval& validityInterval) override;

    /// Sets the controller's value at the specified time.
    virtual void setRotationValue(AnimationTime time, const Rotation& newValue, bool isAbsoluteValue) override;

    /// Lets the rotation controller apply its value to an existing transformation matrix.
    virtual void applyRotation(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval) override;

    /// Computes the largest time interval containing the given time during which the
    /// controller's value is constant.
    virtual TimeInterval validityInterval(AnimationTime time) override;

    /// This asks the controller to adjust its value after a scene node has got a new
    /// parent node.
    ///     oldParentTM - The transformation of the old parent node
    ///     newParentTM - The transformation of the new parent node
    ///     contextNode - The node to which this controller is assigned to
    virtual void changeParent(AnimationTime time, const AffineTransformation& oldParentTM, const AffineTransformation& newParentTM, SceneNode* contextNode) override {}

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override {
        return (rollController() && rollController()->isAnimated())
                || (targetNode() && targetNode()->transformationController() && targetNode()->transformationController()->isAnimated());
    }

private:

    /// The sub-controller for rolling.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, rollController, setRollController);

    /// The target scene node to look at.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SceneNode>, targetNode, setTargetNode, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM);

    /// Stores the cached position of the source node.
    Vector3 _sourcePos;

    /// Stores the validity interval of the saved source position.
    TimeInterval _sourcePosValidity;
};

}   // End of namespace
