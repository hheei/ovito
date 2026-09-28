# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Xml REQUIRED)

SET(SourceFiles
    objects/VoxelGrid.cpp
    objects/VoxelGridVis.cpp
    modifier/CreateIsosurfaceModifier.cpp
    modifier/MarchingCubes.cpp
    io/VTKVoxelGridExporter.cpp
    io/ParaViewVTIGridImporter.cpp
    io/ParaViewVTSGridImporter.cpp
    io/LAMMPSGridDumpImporter.cpp
)

IF(OVITO_BUILD_PLUGIN_STDMOD)
    LIST(APPEND SourceFiles
        modifier/VoxelGridReplicateModifierDelegate.cpp
        modifier/VoxelGridAffineTransformationModifierDelegate.cpp
        modifier/VoxelGridColorCodingModifierDelegate.cpp
        modifier/VoxelGridComputePropertyModifierDelegate.cpp
        modifier/VoxelGridSliceModifierDelegate.cpp
    )
ENDIF()

# Define the plugin module.
OVITO_STANDARD_PLUGIN(Grid
    SOURCES ${SourceFiles}
    PLUGIN_DEPENDENCIES Mesh StdObj
    OPTIONAL_PLUGIN_DEPENDENCIES StdMod
    PRECOMPILED_HEADERS Grid.h
    LIB_DEPENDENCIES Qt6::Xml
)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
