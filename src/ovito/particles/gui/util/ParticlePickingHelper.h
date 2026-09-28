// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/ParticlePrimitive.h>

namespace Ovito {

/**
 * \brief Utility class that supports the picking of particles in the viewports.
 */
class OVITO_PARTICLESGUI_EXPORT ParticlePickingHelper
{
public:

    struct PickResult {

        /// The position of the picked particle in local coordinates.
        Point3 localPos;

        /// The position of the picked particle in world coordinates.
        Point3 worldPos;

        /// The radius of the picked particle.
        FloatType radius;

        /// The index of the picked particle.
        size_t particleIndex;

        /// The identifier of the picked particle.
        IdentifierIntType particleId;

        /// The pipeline scene node that produced the picked particle.
        OORef<SceneNode> sceneNode;
    };

    /// \brief Finds the particle under the mouse cursor.
    /// \param vpwin The viewport window to perform hit testing in.
    /// \param clickPoint The position of the mouse cursor in the viewport.
    /// \param time The animation at which hit testing is performed.
    /// \param result The output structure that receives information about the picked particle.
    /// \return \c true if a particle has been picked; \c false otherwise.
    bool pickParticle(ViewportWindow* vpwin, const QPoint& clickPoint, PickResult& result);

    /// \brief Renders the particle selection overlay in a viewport.
    void renderSelectionMarker(Viewport* vp, FrameGraph& frameGraph, const PickResult& pickRecord);
};

}   // End of namespace
