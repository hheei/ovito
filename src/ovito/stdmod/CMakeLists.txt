# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the plugin module.
OVITO_STANDARD_PLUGIN(StdMod
    SOURCES
        modifiers/SliceModifier.cpp
        modifiers/AffineTransformationModifier.cpp
        modifiers/InvertSelectionModifier.cpp
        modifiers/ClearSelectionModifier.cpp
        modifiers/ColorCodingModifier.cpp
        modifiers/AssignColorModifier.cpp
        modifiers/DeleteSelectedModifier.cpp
        modifiers/SelectTypeModifier.cpp
        modifiers/EditTypesModifier.cpp
        modifiers/EditSimulationCellModifier.cpp
        modifiers/HistogramModifier.cpp
        modifiers/ScatterPlotModifier.cpp
        modifiers/ReplicateModifier.cpp
        modifiers/ExpressionSelectionModifier.cpp
        modifiers/LinesModifierDelegates.cpp
        modifiers/VectorsModifierDelegates.cpp
        modifiers/FreezePropertyModifier.cpp
        modifiers/ManualSelectionModifier.cpp
        modifiers/ComputePropertyModifier.cpp
        modifiers/CombineDatasetsModifier.cpp
        modifiers/ColorByTypeModifier.cpp
        modifiers/RemovePropertyModifier.cpp
        modifiers/TextLabelsModifier.cpp
        viewport/ColorLegendOverlay.cpp
    PLUGIN_DEPENDENCIES
        StdObj
        Mesh
    PRECOMPILED_HEADERS
        StdMod.h
)

# Let the code know that we are building the plugin.
TARGET_COMPILE_DEFINITIONS(StdMod PUBLIC OVITO_BUILD_PLUGIN_STDMOD)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
