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
