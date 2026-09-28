// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

/**
 * \brief Abstract base class for camera objects.
 */
class OVITO_CORE_EXPORT AbstractCameraObject : public DataObject
{
    OVITO_CLASS(AbstractCameraObject)

protected:

    /// Constructor.
    using DataObject::DataObject;

public:

    /// \brief Returns a structure describing the camera's projection.
    /// \param[in] time The animation time for which the camera's projection parameters should be determined.
    /// \param[in,out] projParams The structure that is to be filled with the projection parameters.
    ///     The following fields of the ViewProjectionParameters structure are already filled in when the method is called:
    ///   - ViewProjectionParameters::aspectRatio (The aspect ratio (height/width) of the viewport)
    ///   - ViewProjectionParameters::viewMatrix (The world to view space transformation)
    ///   - ViewProjectionParameters::boundingBox (The bounding box of the scene in world space coordinates)
    virtual void projectionParameters(AnimationTime time, ViewProjectionParameters& projParams) const = 0;

    /// \brief Returns whether this camera uses a perspective projection.
    virtual bool isPerspectiveCamera() const = 0;

    /// \brief Returns the field of view of the camera.
    virtual FloatType fieldOfView(AnimationTime time, TimeInterval& validityInterval) const = 0;
};

}   // End of namespace
