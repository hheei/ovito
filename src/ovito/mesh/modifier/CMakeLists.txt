# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the plugin module.
OVITO_STANDARD_PLUGIN(MeshMod
    SOURCES
        SurfaceMeshSliceModifierDelegate.cpp
        SurfaceMeshAffineTransformationModifierDelegate.cpp
        SurfaceMeshReplicateModifierDelegate.cpp
        SurfaceMeshColorCodingModifierDelegate.cpp
        SurfaceMeshAssignColorModifierDelegate.cpp
        SurfaceMeshDeleteSelectedModifierDelegate.cpp
        SurfaceMeshExpressionSelectionModifierDelegate.cpp
        TriangleMeshAffineTransformationModifierDelegate.cpp
    PLUGIN_DEPENDENCIES Mesh StdMod
)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
