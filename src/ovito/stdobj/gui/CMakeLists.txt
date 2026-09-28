# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the GUI module, which provides the user interface for the parent module.
OVITO_STANDARD_PLUGIN(StdObjGui
    SOURCES
        camera/StandardCameraSourceEditor.cpp
        io/DataTablePlotExporter.cpp
        io/DataTablePlotExporterEditor.cpp
        lines/LinesInspectionApplet.cpp
        lines/LinesVisEditor.cpp
        properties/PropertyInspectionApplet.cpp
        properties/TypesInspectionApplet.cpp
        properties/ElementTypeEditor.cpp
        properties/InputColumnMappingDialog.cpp
        properties/PropertyColorMappingEditor.cpp
        simcell/SimulationCellVisEditor.cpp
        simcell/SimulationCellInspectionApplet.cpp
        table/DataTableInspectionApplet.cpp
        textlabels/TextLabelsVisEditor.cpp
        vectors/VectorsInspectionApplet.cpp
        widgets/PropertyReferenceParameterUI.cpp
        widgets/PropertySelectionComboBox.cpp
        widgets/DataTablePlotWidget.cpp
    PLUGIN_DEPENDENCIES StdObj StdMod
    LIB_DEPENDENCIES Qwt
    PRECOMPILED_HEADERS StdObjGui.h
    GUI_PLUGIN
)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
