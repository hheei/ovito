# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Phase 1 prototype application: renders an OVITO scene inside a Qt Quick window.
# This executable is a technical spike for the migration described in docs/design/UI_PLAN.md.
ADD_EXECUTABLE(OvitoQmlSpike
    Main.cpp
    SpikeHarness.cpp
    checks/ImportChecks.cpp
    checks/RenderingChecks.cpp
    checks/ShellChecks.cpp)

TARGET_LINK_LIBRARIES(OvitoQmlSpike PRIVATE
    GuiQml
    GuiBase
    Core
    Qt6::Core
    Qt6::Gui
    Qt6::Quick)

OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoQmlSpike)

SET_TARGET_PROPERTIES(OvitoQmlSpike PROPERTIES OUTPUT_NAME "ovito-qml-spike")
SET_TARGET_PROPERTIES(OvitoQmlSpike PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
SET_TARGET_PROPERTIES(OvitoQmlSpike PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
IF(APPLE)
    # Note: the plugin and framework directories of an app bundle are relative to Contents/, not to the build
    # directory, so they cannot be built from OVITO_RELATIVE_PLUGINS_DIRECTORY (which is bundle-root relative).
    # Keep this in sync with the main executable target in src/main/CMakeLists.txt.
    SET_TARGET_PROPERTIES(OvitoQmlSpike PROPERTIES INSTALL_RPATH "@executable_path/;@executable_path/../Frameworks/;@executable_path/../PlugIns/")
ELSEIF(LINUX)
    SET_TARGET_PROPERTIES(OvitoQmlSpike PROPERTIES INSTALL_RPATH "$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/lib/:$ORIGIN/../${OVITO_RELATIVE_PLUGINS_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/")
ENDIF()
