# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

IF(WIN32)
    # Always build this module as a static library under Windows, because xdrfile does not explicitly export any symbols.
    # Building as a DLL would require tagging all symbols to be exported with __declspec(dllexport) in the source  code.
    SET(BUILD_SHARED_LIBS OFF)
ENDIF()

# Build library.
ADD_LIBRARY(xdrfile
    xdrfile.c
    xdrfile_xtc.c
    xdrfile_trr.c
)

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(xdrfile PROPERTIES OUTPUT_NAME "ovito_xdrfile")

# xdrfile library does not support unity build. Need to turn it off.
SET_TARGET_PROPERTIES(xdrfile PROPERTIES UNITY_BUILD OFF)

# Make header files of this library available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(xdrfile INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(xdrfile)

IF(CMAKE_C_COMPILER_ID STREQUAL "IntelLLVM")
    # Silence Intel DPC++ compiler warning: "variable XXX set but not used"
    TARGET_COMPILE_OPTIONS(xdrfile PRIVATE "-Wno-unused-but-set-variable")
ENDIF()
IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Silence compiler warning: "warning: 'sprintf' is deprecated: This function is provided for compatibility reasons only."
    TARGET_COMPILE_OPTIONS(xdrfile PRIVATE "-Wno-deprecated-declarations")
ENDIF()

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET xdrfile PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

IF(EMSCRIPTEN)
    # Adopt the Emscript compiler flags from Qt's platform target.
    # That's because all libraries linked into the static WASM module need be built with the same flags.
    TARGET_LINK_LIBRARIES(xdrfile PRIVATE Qt::Platform)
ENDIF()

# Export this target.
INSTALL(TARGETS xdrfile EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
