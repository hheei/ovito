// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include "VoxelGridColorCodingModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridColorCodingModifierDelegate);
OVITO_CLASSINFO(VoxelGridColorCodingModifierDelegate, "DisplayName", "Voxel grids");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> VoxelGridColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all VoxelGrid objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(VoxelGrid::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}   // End of namespace
