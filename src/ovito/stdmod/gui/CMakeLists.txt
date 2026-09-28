# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the GUI module, which provides the user interface for the parent module.
OVITO_STANDARD_PLUGIN(StdModGui
    SOURCES
        SliceModifierEditor.cpp
        AffineTransformationModifierEditor.cpp
        ClearSelectionModifierEditor.cpp
        InvertSelectionModifierEditor.cpp
        ColorCodingModifierEditor.cpp
        AssignColorModifierEditor.cpp
        DeleteSelectedModifierEditor.cpp
        SelectTypeModifierEditor.cpp
        EditTypesModifierEditor.cpp
        EditSimulationCellModifierEditor.cpp
        HistogramModifierEditor.cpp
        ScatterPlotModifierEditor.cpp
        ReplicateModifierEditor.cpp
        ExpressionSelectionModifierEditor.cpp
        FreezePropertyModifierEditor.cpp
        ManualSelectionModifierEditor.cpp
        ComputePropertyModifierEditor.cpp
        CombineDatasetsModifierEditor.cpp
        ColorByTypeModifierEditor.cpp
        ColorLegendOverlayEditor.cpp
        RemovePropertyModifierEditor.cpp
        TextLabelsModifierEditor.cpp
    PLUGIN_DEPENDENCIES StdMod StdObjGui
    PRIVATE_LIB_DEPENDENCIES Qwt
    PRECOMPILED_HEADERS StdModGui.h
    GUI_PLUGIN
    HAS_NO_EXPORTS
)
