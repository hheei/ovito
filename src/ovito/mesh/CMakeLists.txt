# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Xml REQUIRED)

SET(SourceFiles
    surface/SurfaceMesh.cpp
    surface/SurfaceMeshReadAccess.cpp
    surface/SurfaceMeshBuilder.cpp
    surface/SurfaceMeshVis.cpp
    surface/SurfaceMeshTopology.cpp
    surface/SurfaceMeshVertices.cpp
    surface/SurfaceMeshFaces.cpp
    surface/SurfaceMeshRegions.cpp
    io/VTKFileImporter.cpp
    io/VTKTriangleMeshExporter.cpp
    io/ParaViewVTPMeshImporter.cpp
    io/ParaViewVTRSimulationCellImporter.cpp
    io/ParaViewVTUSimulationCellImporter.cpp
    io/ParaViewVTMImporter.cpp
    io/ParaViewPVDImporter.cpp
    io/WavefrontOBJImporter.cpp
    io/STLImporter.cpp
)

# List of source files of the GLU library used for polygon tessellation.
SET(GLUSourceFiles
    util/polytess/dict.c
    util/polytess/geom.c
    util/polytess/memalloc.c
    util/polytess/mesh.c
    util/polytess/normal.c
    util/polytess/priorityq.c
    util/polytess/render.c
    util/polytess/sweep.c
    util/polytess/tess.c
    util/polytess/tessmono.c
)

# Define the plugin module.
OVITO_STANDARD_PLUGIN(Mesh
    SOURCES
        ${SourceFiles}
        ${GLUSourceFiles}
    PLUGIN_DEPENDENCIES StdObj
    PRECOMPILED_HEADERS Mesh.h
    LIB_DEPENDENCIES Qt6::Xml
)

# Let the code know that we are building the plugin.
TARGET_COMPILE_DEFINITIONS(Mesh PUBLIC OVITO_BUILD_PLUGIN_MESH)

# Polytess code does not support unity build. Need to turn it off.
FOREACH(file ${GLUSourceFiles})
    SET_SOURCE_FILES_PROPERTIES(${file} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
ENDFOREACH()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
