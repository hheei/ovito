# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the GUI module, which provides the user interface for the parent module.
OVITO_STANDARD_PLUGIN(CrystalAnalysisGui
    SOURCES
        importer/CAImporterEditor.cpp
        objects/BurgersVectorFamilyEditor.cpp
        objects/MicrostructurePhaseEditor.cpp
        objects/DislocationNetworkEditor.cpp
        objects/DislocationVisEditor.cpp
        modifier/ElasticStrainModifierEditor.cpp
        modifier/GrainSegmentationModifierEditor.cpp
        util/DislocationInspectionApplet.cpp
    PLUGIN_DEPENDENCIES CrystalAnalysis Particles ParticlesGui
    GUI_PLUGIN
    HAS_NO_EXPORTS
)
