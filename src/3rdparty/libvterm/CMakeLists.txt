# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

IF(WIN32)
    # Always build this module as a static library under Windows, because libvterm does not explicitly export any symbols.
    # Building as a DLL would require tagging all symbols to be exported with __declspec(dllexport) in the source code.
    SET(BUILD_SHARED_LIBS OFF)
ENDIF()

# Build library.
ADD_LIBRARY(libvterm
    src/encoding.c
    src/keyboard.c
    src/mouse.c
    src/parser.c
    src/pen.c
    src/screen.c
    src/state.c
    src/unicode.c
    src/vterm.c
)

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(libvterm PROPERTIES OUTPUT_NAME "ovito_vterm")

# libvterm does not support unity build. Need to turn it off.
SET_TARGET_PROPERTIES(libvterm PROPERTIES UNITY_BUILD OFF)

# Make the public header (vterm.h, vterm_keycodes.h) available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(libvterm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(libvterm)

IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # libvterm is written in C99; compile as C, and silence a couple of warnings
    # commonly triggered by its table-driven state machine code.
    TARGET_COMPILE_OPTIONS(libvterm PRIVATE "/wd4244" "/wd4267")
ENDIF()

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET libvterm PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

IF(EMSCRIPTEN)
    # Adopt the Emscript compiler flags from Qt's platform target.
    # That's because all libraries linked into the static WASM module need be built with the same flags.
    TARGET_LINK_LIBRARIES(libvterm PRIVATE Qt::Platform)
ENDIF()

# Export this target.
INSTALL(TARGETS libvterm EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
