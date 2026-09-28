// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>

namespace Ovito {

/**
 * An abstract base class for pipeline sources that generate a camera object.
 */
class OVITO_CORE_EXPORT AbstractCameraSource : public PipelineNode
{
    OVITO_CLASS(AbstractCameraSource)

protected:

    /// Constructor.
    using PipelineNode::PipelineNode;

public:

    /// Returns whether this camera is a target camera directed at a target object.
    virtual bool isTargetCamera() const = 0;

    /// Changes the type of the camera to a target camera or a free camera.
    virtual void setIsTargetCamera(bool enable) = 0;

    /// For a target camera, queries the distance between the camera and its target.
    virtual FloatType targetDistance(AnimationTime time) const = 0;

    /// Returns the current orthogonal field of view.
    virtual FloatType zoom() const = 0;

    /// Sets the field of view of a parallel projection camera.
    virtual void setZoom(FloatType newFOV) = 0;

    /// Returns the current perspective field of view angle.
    virtual FloatType fov() const = 0;

    /// Sets the field of view angle of a perspective projection camera.
    virtual void setFov(FloatType newFOV) = 0;

    /// Returns whether this camera uses a perspective projection.
    virtual bool isPerspectiveCamera() const = 0;

    /// Lets the source generate a camera object for the given animation time.
    virtual DataOORef<const AbstractCameraObject> cameraObject(AnimationTime time) const = 0;
};

}   // End of namespace
