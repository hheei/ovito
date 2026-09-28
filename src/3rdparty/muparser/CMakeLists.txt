# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Build library.
ADD_LIBRARY(muParser
    muParser.cpp
    muParserBytecode.cpp
    muParserInt.cpp
    muParserTokenReader.cpp
    muParserError.cpp
    muParserCallback.cpp
    muParserBase.cpp
)

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(muParser PROPERTIES OUTPUT_NAME "ovito_muparser")

# Define macro for symbol export from the shared library.
INCLUDE(GenerateExportHeader)
GENERATE_EXPORT_HEADER(muParser BASE_NAME MUPARSER)
TARGET_INCLUDE_DIRECTORIES(muParser PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

# Make sure the muParser library gets compiled with the same standard flags as the other OVITO modules, which actively
# use the Qt framework. In particular, Qt 6 requires UNICODE strings on Windows platform.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)
TARGET_LINK_LIBRARIES(muParser PUBLIC Qt6::Core)

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(muParser)

IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Do not warn about missing dll-interface of members of an exported class.
    TARGET_COMPILE_OPTIONS(muParser PRIVATE "/wd4251")
    # Do not warn about missing dll-interface of base class of an exported class.
    TARGET_COMPILE_OPTIONS(muParser PRIVATE "/wd4275")
ENDIF()

# Make header files of this library available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(muParser INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET muParser PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

# Export this target.
INSTALL(TARGETS muParser EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
