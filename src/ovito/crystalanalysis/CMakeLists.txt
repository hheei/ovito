# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

SET(SourceFiles
    importer/CAImporter.cpp
    exporter/CAExporter.cpp
    exporter/VTKDislocationsExporter.cpp
    objects/BurgersVectorFamily.cpp
    objects/ClusterGraph.cpp
    objects/DislocationNetwork.cpp
    objects/DislocationVis.cpp
    objects/MicrostructurePhase.cpp
    modifier/structureanalysis/StructureAnalysis.cpp
    modifier/elasticstrain/ElasticStrainModifier.cpp
    modifier/elasticstrain/ElasticStrainEngine.cpp
    modifier/grains/GrainSegmentationModifier.cpp
    modifier/grains/GrainSegmentationEngine.cpp
    modifier/grains/NodePairSampling.cpp
)

IF(OVITO_BUILD_PLUGIN_STDMOD)
    LIST(APPEND SourceFiles
        objects/DislocationSliceModifierDelegate.cpp
        objects/DislocationReplicateModifierDelegate.cpp
        objects/DislocationAffineTransformationModifierDelegate.cpp
        )
ENDIF()

# Define the plugin module.
OVITO_STANDARD_PLUGIN(CrystalAnalysis
    SOURCES ${SourceFiles}
    PLUGIN_DEPENDENCIES Particles Mesh StdObj Grid Delaunay
    PRIVATE_LIB_DEPENDENCIES PolyhedralTemplateMatching
    OPTIONAL_PLUGIN_DEPENDENCIES StdMod
    PRECOMPILED_HEADERS CrystalAnalysis.h
)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
