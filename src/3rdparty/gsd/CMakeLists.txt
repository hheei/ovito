# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

IF(WIN32)
    # Always build this module as a static library under Windows, because gsd does not explicitly export any symbols.
    # Building as a DLL would require tagging all symbols to be exported with __declspec(dllexport) in the source  code.
    SET(BUILD_SHARED_LIBS OFF)
ENDIF()

# Build library.
ADD_LIBRARY(gsd
    gsd.c
)

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(gsd PROPERTIES OUTPUT_NAME "ovito_gsd")

# gsd library does not support unity build. Need to turn it off.
SET_TARGET_PROPERTIES(gsd PROPERTIES UNITY_BUILD OFF)

# Make header files of this library available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(gsd INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(gsd)

IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Suppress warning on conversion from size_t to int, possible loss of data.
    TARGET_COMPILE_OPTIONS(gsd PRIVATE "/wd4244")
    # Suppress warning on signed/unsigned mismatch.
    TARGET_COMPILE_OPTIONS(gsd PRIVATE "/wd4018")
ENDIF()

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET gsd PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

IF(EMSCRIPTEN)
    # Adopt the Emscript compiler flags from Qt's platform target.
    # That's because all libraries linked into the static WASM module need be built with the same flags.
    TARGET_LINK_LIBRARIES(gsd PRIVATE Qt::Platform)
ENDIF()

# Export this target.
INSTALL(TARGETS gsd EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
