# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define plugin module.
OVITO_STANDARD_PLUGIN(GuiBase
    SOURCES
        app/GuiFrontendRegistry.cpp
        app/WorkbenchUI.cpp
        actions/ActionManager.cpp
        actions/ViewportModeAction.cpp
        actions/ViewportActions.cpp
        actions/AnimationActions.cpp
        mainwin/AvailableModifiersModel.cpp
        mainwin/AvailableOverlaysModel.cpp
        mainwin/OverlayListModel.cpp
        mainwin/OverlayListItem.cpp
        mainwin/PipelineListModel.cpp
        mainwin/PipelineListItem.cpp
        mainwin/UtilityObject.cpp
        mainwin/templates/ModifierTemplates.cpp
        mainwin/templates/OverlayTemplates.cpp
        mainwin/templates/ObjectTemplates.cpp
        viewport/ViewportInputManager.cpp
        viewport/ViewportInputMode.cpp
        viewport/NavigationModes.cpp
        viewport/SelectionMode.cpp
        viewport/BaseViewportWindow.cpp
        resources/guibase.qrc
    PRECOMPILED_HEADERS
        GUIBase.h
)

IF(NOT EMSCRIPTEN)
    # Compute the relative path where the documentation files get installed relative to the executable.
    FILE(RELATIVE_PATH OVITO_DOCUMENTATION_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_SHARE_DIRECTORY}/doc/manual/html/")
    TARGET_COMPILE_DEFINITIONS(GuiBase PRIVATE "OVITO_DOCUMENTATION_PATH=\"${OVITO_DOCUMENTATION_PATH}\"")
ELSE()
    # Download the documentation inventory file before creating the resource file.
    FILE(DOWNLOAD https://docs.ovito.org/objects.txt "${CMAKE_CURRENT_BINARY_DIR}/objects.txt")
    # Link the manual inventory file into the executable.
    QT_ADD_RESOURCES(GuiBase PackagedManual
        PREFIX "/doc/manual/"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES "${CMAKE_CURRENT_BINARY_DIR}/objects.txt")
ENDIF()

# Compute the relative path where the Python extension scripts get installed relative to the executable
# and pass this relative path to the source code, which uses it for different purposes.
FILE(RELATIVE_PATH OVITO_SCRIPT_EXTENSIONS_RELATIVE_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_SHARE_DIRECTORY}/scripts/")
TARGET_COMPILE_DEFINITIONS(GuiBase PUBLIC "OVITO_SCRIPT_EXTENSIONS_RELATIVE_PATH=\"${OVITO_SCRIPT_EXTENSIONS_RELATIVE_PATH}\"")

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
