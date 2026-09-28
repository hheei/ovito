# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Place all compiled libraries into OVITO's library directory by default.
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OVITO_LIBRARY_DIRECTORY}")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OVITO_LIBRARY_DIRECTORY}")

# Build all third-party libraries.
ADD_SUBDIRECTORY(3rdparty)

# Build all plugin modules including the core module.
ADD_SUBDIRECTORY(ovito)

# This allows external CMake projects to inject their customization code into the OVITO project:
IF(OVITO_CMAKE_FILE_INJECTION_PLUGINS)
    INCLUDE("${OVITO_CMAKE_FILE_INJECTION_PLUGINS}")
ENDIF()

# Build the application executable 'ovito'.
IF(OVITO_BUILD_APP)
    ADD_SUBDIRECTORY(main)
ENDIF()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
