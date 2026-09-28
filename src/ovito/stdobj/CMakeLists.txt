# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the plugin module.
OVITO_STANDARD_PLUGIN(StdObj
    SOURCES
        camera/StandardCameraObject.cpp
        camera/StandardCameraSource.cpp
        camera/TargetObject.cpp
        io/DataTableExporter.cpp
        io/PropertyOutputWriter.cpp
        io/StandardFrameLoader.cpp
        lines/Lines.cpp
        lines/LinesVis.cpp
        properties/Property.cpp
        properties/OwnerPropertyRef.cpp
        properties/PropertyReference.cpp
        properties/PropertyContainerClass.cpp
        properties/PropertyContainer.cpp
        properties/ElementType.cpp
        properties/ElementTypeClass.cpp
        properties/GenericPropertyModifier.cpp
        properties/PropertyExpressionEvaluator.cpp
        properties/PropertyExpressionRewriter.cpp
        properties/InputColumnMapping.cpp
        properties/PropertyColorMapping.cpp
        simcell/SimulationCell.cpp
        simcell/SimulationCellVis.cpp
        simcell/PeriodicDomainObject.cpp
        table/DataTable.cpp
        textlabels/TextLabelsVis.cpp
        util/ElementSelectionSet.cpp
        vectors/Vectors.cpp
        vectors/VectorVis.cpp
    LIB_DEPENDENCIES
        muParser
    PRECOMPILED_HEADERS
        StdObj.h
)

# Let the code know that we are building the plugin.
TARGET_COMPILE_DEFINITIONS(StdObj PUBLIC OVITO_BUILD_PLUGIN_STDOBJ)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
