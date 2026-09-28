# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

IF(WIN32)
    # Always build this module as a static library under Windows, because Voro++ does not explicitly export any symbols.
    # Building as a DLL would require tagging all symbols to be exported with __declspec(dllexport) in the source  code.
    SET(BUILD_SHARED_LIBS OFF)
ENDIF()

# Build library.
ADD_LIBRARY(MinimumWeightMatchingCentrosymmetry
    matching.cpp
    mwm_csp.cpp
)

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(MinimumWeightMatchingCentrosymmetry)

# Simply export all symbols defined by the library, because the code doesn't explicitly export certain API symbols.
SET_TARGET_PROPERTIES(MinimumWeightMatchingCentrosymmetry PROPERTIES CXX_VISIBILITY_PRESET "default")

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(MinimumWeightMatchingCentrosymmetry PROPERTIES OUTPUT_NAME "ovito_mwm_csp")

# Make header files of this library available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(MinimumWeightMatchingCentrosymmetry INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET MinimumWeightMatchingCentrosymmetry PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

# Export this target.
INSTALL(TARGETS MinimumWeightMatchingCentrosymmetry EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
