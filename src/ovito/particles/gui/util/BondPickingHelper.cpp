// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/objects/BondsVis.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include "BondPickingHelper.h"

namespace Ovito {

/******************************************************************************
* Finds the bond under the mouse cursor.
******************************************************************************/
bool BondPickingHelper::pickBond(ViewportWindow* vpwin, const QPoint& clickPoint, PickResult& result)
{
    // Check if user has clicked on something.
    if(std::optional<ViewportWindow::PickResult> vpPickResult = vpwin->pick(clickPoint)) {

        // Check if that was a bond.
        if(BondPickInfo* pickInfo = dynamic_object_cast<BondPickInfo>(vpPickResult->pickInfo().get())) {
            if(pickInfo->particles()->bonds()) {
                size_t bondIndex = vpPickResult->subobjectId() / 2;
                const Property* topologyProperty = pickInfo->particles()->bonds()->getTopology();
                if(topologyProperty && topologyProperty->size() > bondIndex) {
                    // Save reference to the selected bond.
                    result.sceneNode = vpPickResult->sceneNode();
                    result.bondIndex = bondIndex;
                    return true;
                }
            }
        }
    }

    result.sceneNode = nullptr;
    return false;
}

}   // End of namespace
