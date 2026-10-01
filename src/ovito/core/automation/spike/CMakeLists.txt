# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Phase 2.6, deliverable 6: the interpreter and execution-topology spike.
#
# Like the Qt Quick spike of Phase 1, this executable is a technical prototype and not a product entry point: it starts a
# user-selected Python interpreter, drives the worker protocol over two pipes, and reports what the seam costs, so that
# the topology decision of docs/design/AUTOMATION_TOPOLOGY_SPIKE.md rests on measurements rather than on an assumption.
# The worker and the framing live here until Phase 4 implements the chosen backend, and the spike has no install rule:
# it is built for the developer and the CI job that records its numbers.
ADD_EXECUTABLE(OvitoAutomationSpike
    SpikeMain.cpp
    WorkerProcess.cpp)

TARGET_LINK_LIBRARIES(OvitoAutomationSpike PRIVATE
    Core
    Qt6::Core)

OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoAutomationSpike)

# The spike finds the worker script next to the probe script of the Python contract, the same way the probe finds its own
# script (see the note on OVITO_AUTOMATION_PYTHON_DIR in src/ovito/core/CMakeLists.txt).
SET_PROPERTY(SOURCE SpikeMain.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OVITO_AUTOMATION_PYTHON_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../python\"")

SET_TARGET_PROPERTIES(OvitoAutomationSpike PROPERTIES OUTPUT_NAME "ovito-automation-spike")
SET_TARGET_PROPERTIES(OvitoAutomationSpike PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
SET_TARGET_PROPERTIES(OvitoAutomationSpike PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
IF(APPLE)
    SET_TARGET_PROPERTIES(OvitoAutomationSpike PROPERTIES INSTALL_RPATH "@executable_path/;@executable_path/../Frameworks/;@executable_path/../PlugIns/")
ELSEIF(LINUX)
    SET_TARGET_PROPERTIES(OvitoAutomationSpike PROPERTIES INSTALL_RPATH "$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/lib/:$ORIGIN/../${OVITO_RELATIVE_PLUGINS_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/")
ENDIF()

# Phase 2.6, deliverable 8: the local-protocol spike.
#
# The second prototype executable of the automation module: it hosts the automation session behind the local-only JSON
# Lines endpoint of Core, publishes a session descriptor for discovery, and drives itself as a client to check every rule
# the endpoint claims to enforce. The executable is a developer tool without an install rule; what it exercises is the
# transport the command line of `ovito --automation` uses.
#
# Qt6::Gui is linked for QImage alone: the artifact checks need PNG encoding, not a window, and this program never
# creates a graphics device (see the note on capture in IpcSpikeMain.cpp). Qt Network provides the local socket, and is
# the one Qt module Core does not link unless the Python support asks for it, so it is found here.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Network REQUIRED)

# The endpoint and the client it drives live in Core since Phase 3 (audit decision D67), which is what makes this
# executable a self-test of the shipped transport rather than of a private clone of it.
ADD_EXECUTABLE(OvitoAutomationIpcSpike
    IpcSpikeMain.cpp)

TARGET_LINK_LIBRARIES(OvitoAutomationIpcSpike PRIVATE
    Core
    Qt6::Core
    Qt6::Gui
    Qt6::Network)

OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoAutomationIpcSpike)

# The classes of the endpoint and the client are QObjects with signals, so the executable moc's its own sources.
SET_TARGET_PROPERTIES(OvitoAutomationIpcSpike PROPERTIES AUTOMOC TRUE)

SET_TARGET_PROPERTIES(OvitoAutomationIpcSpike PROPERTIES OUTPUT_NAME "ovito-automation-ipc-spike")
SET_TARGET_PROPERTIES(OvitoAutomationIpcSpike PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
SET_TARGET_PROPERTIES(OvitoAutomationIpcSpike PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
IF(APPLE)
    SET_TARGET_PROPERTIES(OvitoAutomationIpcSpike PROPERTIES INSTALL_RPATH "@executable_path/;@executable_path/../Frameworks/;@executable_path/../PlugIns/")
ELSEIF(LINUX)
    SET_TARGET_PROPERTIES(OvitoAutomationIpcSpike PROPERTIES INSTALL_RPATH "$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/lib/:$ORIGIN/../${OVITO_RELATIVE_PLUGINS_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/")
ENDIF()
