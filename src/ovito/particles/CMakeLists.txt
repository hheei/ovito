# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Xml REQUIRED)

# SQLite3 is required by the MedeA SLI importer.
# In official release packages and PyPI wheels, SQLite3 is built from bundled
# sources (see src/3rdparty/sqlite/), providing an in-tree static library target
# In all other builds (conda, developer workstations,
# distro system-package builds), locate SQLite3 from the environment.
IF(NOT OVITO_REDISTRIBUTABLE_PACKAGE AND NOT OVITO_BUILD_PYPI)
    OVITO_FIND_PACKAGE(SQLite3 REQUIRED)
    IF(NOT TARGET SQLite3::SQLite3) # For backward compatibility with CMake < 4.3
        ADD_LIBRARY(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
    ENDIF()
ENDIF()

SET(SourceFiles
    objects/ParticlesVis.cpp
    objects/ParticleType.cpp
    objects/Particles.cpp
    objects/BondType.cpp
    objects/ParticleBondMap.cpp
    objects/Bonds.cpp
    objects/BondsVis.cpp
    objects/Angles.cpp
    objects/Dihedrals.cpp
    objects/Impropers.cpp
    modifier/visualization/AmbientOcclusionModifier.cpp
    modifier/modify/WrapPeriodicImagesModifier.cpp
    modifier/modify/UnwrapTrajectoriesModifier.cpp
    modifier/modify/CreateBondsModifier.cpp
    modifier/modify/LoadTrajectoryModifier.cpp
    modifier/modify/CoordinationPolyhedraModifier.cpp
    modifier/analysis/StructureIdentificationModifier.cpp
    modifier/analysis/ReferenceConfigurationModifier.cpp
    modifier/analysis/ackland_jones/AcklandJonesModifier.cpp
    modifier/analysis/cna/CommonNeighborAnalysisModifier.cpp
    modifier/analysis/rdf/RadialDistributionFunctionModifier.cpp
    modifier/analysis/chill_plus/ChillPlusModifier.cpp
    modifier/analysis/displacements/CalculateDisplacementsModifier.cpp
    modifier/analysis/centrosymmetry/CentroSymmetryModifier.cpp
    modifier/analysis/strain/AtomicStrainModifier.cpp
    modifier/analysis/diamond/IdentifyDiamondModifier.cpp
    modifier/analysis/voronoi/VoronoiAnalysisModifier.cpp
    modifier/analysis/cluster/ClusterAnalysisModifier.cpp
    modifier/analysis/wignerseitz/WignerSeitzAnalysisModifier.cpp
    modifier/analysis/ptm/PolyhedralTemplateMatchingModifier.cpp
    modifier/analysis/ptm/PTMAlgorithm.cpp
    modifier/analysis/surface/ConstructSurfaceModifier.cpp
    modifier/selection/ExpandSelectionModifier.cpp
    modifier/selection/FindOverlappingParticlesModifier.cpp
    modifier/properties/SmoothTrajectoryModifier.cpp
    modifier/properties/GenerateTrajectoryLinesModifier.cpp
    import/ParticleImporter.cpp
    import/lammps/LAMMPSTextDumpImporter.cpp
    import/lammps/LAMMPSBinaryDumpImporter.cpp
    import/lammps/LAMMPSDataImporter.cpp
    import/lammps/LAMMPSDumpLocalImporter.cpp
    import/lammps/LAMMPSDumpYAMLImporter.cpp
    import/lammps/ReaxFFBondImporter.cpp
    import/vasp/POSCARImporter.cpp
    import/vasp/VASPHDF5Importer.cpp
    import/netcdf/AMBERNetCDFImporter.cpp
    import/xyz/XYZImporter.cpp
    import/imd/IMDImporter.cpp
    import/parcas/ParcasFileImporter.cpp
    import/cfg/CFGImporter.cpp
    import/pdb/PDBImporter.cpp
    import/sdf/SDFImporter.cpp
    import/mol2/MOL2Importer.cpp
    import/MedeA/MedeA_SCI_Importer.cpp
    import/MedeA/MedeA_SLI_Importer.cpp
    import/fhi_aims/FHIAimsImporter.cpp
    import/fhi_aims/FHIAimsLogFileImporter.cpp
    import/gsd/GSDImporter.cpp
    import/castep/CastepCellImporter.cpp
    import/castep/CastepMDImporter.cpp
    import/cube/GaussianCubeImporter.cpp
    import/xsf/XSFImporter.cpp
    import/dcd/DCDImporter.cpp
    import/dl_poly/DLPOLYImporter.cpp
    import/quantumespresso/QuantumEspressoImporter.cpp
    import/cif/CIFImporter.cpp
    import/cif/mmCIFImporter.cpp
    import/gromacs/GroImporter.cpp
    import/gromacs/XTCImporter.cpp
    import/gromacs/TRRImporter.cpp
    import/vtk/ParaViewVTPParticleImporter.cpp
    import/vtk/ParaViewVTPParticleParticleContactsImporter.cpp
    import/vtk/ParaViewVTPParticleWallContactsImporter.cpp
    import/mercurydpm/MercuryDPMImporter.cpp
    export/ParticleExporter.cpp
    export/FileColumnParticleExporter.cpp
    export/vasp/POSCARExporter.cpp
    export/netcdf/AMBERNetCDFExporter.cpp
    export/fhi_aims/FHIAimsExporter.cpp
    export/lammps/LAMMPSDataExporter.cpp
    export/lammps/LAMMPSDumpExporter.cpp
    export/xyz/XYZExporter.cpp
    export/imd/IMDExporter.cpp
    export/gsd/GSDExporter.cpp
    util/NearestNeighborFinder.cpp
    util/CutoffNeighborFinder.cpp
    util/PTMNeighborFinder.cpp
    util/ParticleExpressionEvaluator.cpp
)

IF(OVITO_BUILD_PLUGIN_STDMOD)
    LIST(APPEND SourceFiles
        modifier/visualization/ParticlesColorCodingModifierDelegate.cpp
        modifier/visualization/ParticlesAssignColorModifierDelegate.cpp
        modifier/modify/ParticlesSliceModifierDelegate.cpp
        modifier/modify/ParticlesAffineTransformationModifierDelegate.cpp
        modifier/modify/ParticlesDeleteSelectedModifierDelegate.cpp
        modifier/modify/ParticlesReplicateModifierDelegate.cpp
        modifier/properties/ParticlesComputePropertyModifierDelegate.cpp
        modifier/properties/BondsComputePropertyModifierDelegate.cpp
        modifier/selection/ParticlesExpressionSelectionModifierDelegate.cpp
        modifier/modify/ParticlesCombineDatasetsModifierDelegate.cpp
    )
ENDIF()

IF(OVITO_USE_SYCL)
    LIST(APPEND SourceFiles
        util/SyclNeighborFinderBase.cpp
        util/SyclCutoffNeighborFinder.cpp
#        util/SyclNearestNeighborFinder.cpp
    )
ENDIF()

# Define the plugin module.
OVITO_STANDARD_PLUGIN(Particles
    SOURCES ${SourceFiles}
    PLUGIN_DEPENDENCIES Mesh Grid StdObj Delaunay
    PRIVATE_LIB_DEPENDENCIES VoroPlusPlus PolyhedralTemplateMatching MinimumWeightMatchingCentrosymmetry xdrfile gemmi gsd SQLite3::SQLite3 NetCDFIntegration
    OPTIONAL_PLUGIN_DEPENDENCIES StdMod
    PRECOMPILED_HEADERS Particles.h
    LIB_DEPENDENCIES Qt6::Xml
)

# Let the code know that we are building the plugin.
TARGET_COMPILE_DEFINITIONS(Particles PUBLIC OVITO_BUILD_PLUGIN_PARTICLES)

# The VASP HDF5 (vaspout.h5) file reader uses the HDF5 C library. OVITO accesses HDF5 through the
# NetCDFIntegration module (linked above), which publicly exposes the HDF5 headers and library and
# provides the shared initializeHDF5Library() helper.

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)