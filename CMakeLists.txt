# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Make sure we have a recent version of CMake.
CMAKE_MINIMUM_REQUIRED(VERSION 3.25.0 FATAL_ERROR)
CMAKE_POLICY(VERSION 3.12.0)

# Choose CMake policies.
CMAKE_POLICY(SET CMP0100 NEW)   # Let AUTOMOC and AUTOUIC process .hh files.
CMAKE_POLICY(SET CMP0099 NEW)   # Link properties are transitive over private dependency on static libraries.
CMAKE_POLICY(SET CMP0079 NEW)   # Allows use of target_link_libraries() with targets defined in other directories.
CMAKE_POLICY(SET CMP0104 NEW)   # Initialize CMAKE_CUDA_ARCHITECTURES when CMAKE_CUDA_COMPILER_ID is NVIDIA. Raise an error if CUDA_ARCHITECTURES is empty.
IF(CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
    CMAKE_POLICY(SET CMP0144 NEW)   # find_package() uses upper-case <PACKAGENAME>_ROOT variables.
ENDIF()
IF(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    CMAKE_POLICY(SET CMP0167 OLD)   # find_package(Boost) should load CMake's FindBoost module for now. We'll switch to the new policy later.
ENDIF()
IF(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31")
    CMAKE_POLICY(SET CMP0177 NEW)   # Normalize install() DESTINATION paths.
ENDIF()

PROJECT(Ovito)

# Build acceleration: ccache & mold linker
OPTION(OVITO_USE_CCACHE "Enable ccache compiler launcher if available" ON)
IF(OVITO_USE_CCACHE)
    # An explicitly configured launcher wins. The CI presets select ccache or sccache through the variable
    # OVITO_COMPILER_LAUNCHER (see CMAKE_CXX_COMPILER_LAUNCHER in CMakePresets.json); overriding that here would
    # silently ignore the choice, and an empty value could not be used to ask for a build without a compiler cache.
    IF(NOT DEFINED CMAKE_C_COMPILER_LAUNCHER AND NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
        FIND_PROGRAM(CCACHE_PROGRAM ccache)
        IF(CCACHE_PROGRAM)
            MESSAGE(STATUS "Build speedup: ccache enabled (${CCACHE_PROGRAM})")
            SET(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
            SET(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        ENDIF()
    ENDIF()
ENDIF()

OPTION(OVITO_USE_MOLD "Enable mold linker if available" ON)
IF(OVITO_USE_MOLD AND NOT WIN32 AND NOT APPLE)
    FIND_PROGRAM(MOLD_PROGRAM mold)
    IF(MOLD_PROGRAM)
        MESSAGE(STATUS "Build speedup: mold linker enabled (${MOLD_PROGRAM})")
        ADD_LINK_OPTIONS("-fuse-ld=mold")
    ENDIF()
ENDIF()

# This is to enable target debugging within Visual Studio Code.
# It is ignored outside of Visual Studio Code/CMake tools.
INCLUDE(CMakeToolsHelpers OPTIONAL)

# Builds a global list of third-party packages and version for SBOM generation.
MACRO(OVITO_REGISTER_PACKAGE_FOR_SBOM package_name package_version)
    # Get the current lists of package names and package versions.
    GET_PROPERTY(_package_names GLOBAL PROPERTY OVITO_PACKAGE_NAME_LIST_FOR_SBOM)
    GET_PROPERTY(_package_versions GLOBAL PROPERTY OVITO_PACKAGE_VERSION_LIST_FOR_SBOM)

    # Add the new package if not already in the list
    IF(NOT "${package_name}" IN_LIST _package_names)
        LIST(APPEND _package_names "${package_name}")
        LIST(APPEND _package_versions "${package_version}")
        SET_PROPERTY(GLOBAL PROPERTY OVITO_PACKAGE_NAME_LIST_FOR_SBOM "${_package_names}")
        SET_PROPERTY(GLOBAL PROPERTY OVITO_PACKAGE_VERSION_LIST_FOR_SBOM "${_package_versions}")
    ENDIF()
ENDMACRO()

# Wrapper for CMake's find_package() that tracks found packages
# and builds a list of them for SBOM generation.
MACRO(OVITO_FIND_PACKAGE)
    # Call the original find_package with all arguments
    FIND_PACKAGE(${ARGV})

    # If the package was found, add it to our tracking list
    IF(${ARGV0}_FOUND)
        # Try to get the version variable for the package
        # and add the new package if not already in the list
        SET(_version_var "${ARGV0}_VERSION")
        IF(DEFINED ${_version_var})
            OVITO_REGISTER_PACKAGE_FOR_SBOM("${ARGV0}" "${${_version_var}}")
        ELSE()
            OVITO_REGISTER_PACKAGE_FOR_SBOM("${ARGV0}" "unknown")
        ENDIF()
    ENDIF()
ENDMACRO()

IF(EMSCRIPTEN)
    INCLUDE(cmake/wasm.cmake)
ENDIF()

SET(OVITO_SOURCE_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
LIST(APPEND CMAKE_MODULE_PATH "${OVITO_SOURCE_BASE_DIR}/cmake")
INCLUDE(cmake/Version.cmake)
INCLUDE(cmake/Plugins.cmake)
INCLUDE(cmake/SYCL.cmake)
INCLUDE(cmake/Cuda.cmake)

# Define build options.
OPTION(OVITO_BUILD_APP "Build the OVITO desktop application with a GUI." "ON")
OPTION(OVITO_BUILD_QML_FRONTEND "Build the experimental Qt Quick / QML frontend module and its prototype application." "OFF")
OPTION(OVITO_BUILD_MONOLITHIC "Build the application as a monolithic executable, statically linking in all plugins." "OFF")
OPTION(OVITO_REDISTRIBUTABLE_PACKAGE "Create a redistributable program package that includes third-party libraries." "OFF")
SET(OVITO_VULKAN_FALLBACK_DIR "/opt/vulkan-fallback" CACHE PATH
    "Directory holding the software Vulkan driver (Mesa lavapipe) that is shipped with the redistributable Linux package as a fallback for systems without a Vulkan driver.")
OPTION(OVITO_BUILD_APPSTORE_VERSION "Build binaries for an app store. This option disables integrated software update checks." "OFF")
OPTION(OVITO_RUN_CLANG_TIDY "Run the clang-tidy tool to check code." "OFF")
OPTION(OVITO_USE_ADDRESS_SANITIZER "Builds the code with AddressSanitizer support to detect memory errors (only in debug builds)." "OFF")
OPTION(OVITO_USE_THREAD_SANITIZER "Builds the code with ThreadSanitizer support to detect race condition errors (only in debug builds)." "OFF")
OPTION(OVITO_USE_PRECOMPILED_HEADERS "Use precompiled C++ headers to speed up build." "ON")
OPTION(OVITO_USE_UNITY_BUILD "Use unity builds to speed up compilation." "OFF")
OPTION(OVITO_DISABLE_THREADING "Disable multi-threading code (meant only for development purposes)." "OFF")
OPTION(OVITO_DOUBLE_PRECISION_FP "Use double-precision floating-point numbers." "ON")
OPTION(OVITO_IS_DEVELOPMENT_BUILD "Build a development version of OVITO." "OFF")
OPTION(OVITO_BUILD_SSH_CLIENT "Build the integrated SSH client for remote file access (requires libssh)." "OFF")
OPTION(OVITO_USE_SYCL "Build with SYCL support" "OFF")
SET(OVITO_USE_SYCL None CACHE STRING "SYCL implementation to use")
SET_PROPERTY(CACHE OVITO_USE_SYCL PROPERTY STRINGS OFF AdaptiveCpp DPC++)
OPTION(OVITO_USE_CUDA "Build with native CUDA support" "OFF")

# Define user options that control the building of OVITO's standard plugins.
OPTION(OVITO_BUILD_PLUGIN_STDOBJ "Build the standard objects plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_STDMOD "Build the standard modifiers plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_PARTICLES "Build the plugin for particle-related functions." "ON")
OPTION(OVITO_BUILD_PLUGIN_MESH "Build the Mesh plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_GRID "Build the Grid plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_CRYSTALANALYSIS "Build the CrystalAnalysis plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_CORRELATION "Build the spatial correlation function modifier plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_VOROTOP "Build the VoroTop modifier plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_GALAMOST "Build the GALAMOST I/O plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_OXDNA "Build the oxDNA I/O plugin." "ON")

# This is a global list of plugin targets that will be built.
# It will get populated by the OVITO_PLUGIN function.
SET(OVITO_PLUGINS_LIST "")

# OVITO needs Qt 6.10+.
SET(OVITO_MINIMUM_REQUIRED_QT_VERSION 6.10)
SET(QT_NO_PRIVATE_MODULE_WARNING ON) # Suppress warning about Qt::GuiPrivate being a module with private APIs.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)

# Enable CUDA support (optional)
IF(OVITO_USE_CUDA)
    ENABLE_LANGUAGE(CUDA)
    OVITO_FIND_PACKAGE(CUDAToolkit REQUIRED)
    SET(CMAKE_CUDA_STANDARD 20)
    SET(CMAKE_CUDA_STANDARD_REQUIRED TRUE)
    SET(CMAKE_CUDA_SEPARABLE_COMPILATION TRUE)
ENDIF()

# Enable software testing framework.
ENABLE_TESTING()
INCLUDE(CTest)
INCLUDE(cmake/OvitoTesting.cmake)

# Add C++ unit tests (only when BUILD_TESTING and Qt6::Test are available).
IF(BUILD_TESTING AND OVITO_BUILD_CPP_TESTS)
    ADD_SUBDIRECTORY(tests)
ENDIF()

# Activate C++23 language standard.
SET(CMAKE_CXX_STANDARD 23)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set visibility of symbols in libraries to hidden by default, except those exported in the source code.
IF(NOT OVITO_USE_SYCL STREQUAL AdaptiveCpp)
    SET(CMAKE_CXX_VISIBILITY_PRESET "hidden")
    SET(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
ENDIF()

# Enable link time optimization (LTO)
IF(WIN32 OR APPLE) # On Linux, LTO currently breaks the executable due to missing symbols at runtime.
    IF(${CMAKE_BUILD_TYPE} MATCHES Release)
        INCLUDE(CheckIPOSupported)
        check_ipo_supported(RESULT result OUTPUT output)
        IF(result)
            MESSAGE(STATUS "Using link-time optimization (LTO)")
            SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        ENDIF()
    ENDIF()
ENDIF()

# Set the name of the macOS application bundle to generate.
SET(MACOSX_BUNDLE_NAME "Ovito")
SET(MACOSX_BUNDLE_BUNDLE_NAME "${MACOSX_BUNDLE_NAME}")

IF(NOT OVITO_BUILD_PYPI)
    IF(LINUX)
        # The directory where the main executable goes to.
        SET(OVITO_RELATIVE_BINARY_DIRECTORY "bin")
        # The directory where the main libraries go to.
        SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "lib/ovito")
        # The directory where the third-party libraries go to.
        SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "lib/ovito")
        # The directory where the auxiliary files go to.
        SET(OVITO_RELATIVE_SHARE_DIRECTORY "share/ovito")
        # The directory where the compiled plugins go to.
        SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins")
        # The directory where the Python source modules go to.
        IF(NOT OVITO_BUILD_CONDA)
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "${OVITO_RELATIVE_PLUGINS_DIRECTORY}/python")
        ELSE()
            # For Conda package builds:
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "$ENV{SP_DIR}")
        ENDIF()
    ELSEIF(APPLE)
        IF(NOT OVITO_BUILD_CONDA)
            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/MacOS")
            # The directory where the main libraries of Ovito go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/MacOS")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/Resources")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/PlugIns")
            # The directory where the Python source modules go to.
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/Resources/python")
        ELSE()
            # For Conda package builds:

            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY "bin")
            # The directory where the main libraries go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "lib/ovito")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY "share/ovito")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins")
            # The directory where the Python source modules go to.
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "$ENV{SP_DIR}")
        ENDIF()
    ELSEIF(WIN32)
        IF(NOT OVITO_BUILD_CONDA)
            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY ".")
            # The directory where the main libraries go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY ".")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY ".")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY ".")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the Python source modules go to.
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "./plugins/python")
        ELSE()
            # For Conda package builds:

            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY "bin")
            # The directory where the main libraries go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "bin")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY "share/ovito")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the Python source modules go to.
            FILE(TO_CMAKE_PATH "$ENV{SP_DIR}" OVITO_RELATIVE_PYTHON_DIRECTORY)
        ENDIF()
    ENDIF()
ELSE()
    # For PyPI wheel builds:

    # The directory where the main executable goes to.
    SET(OVITO_RELATIVE_BINARY_DIRECTORY "ovito")
    # The directory where the main libraries go to.
    SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "ovito/plugins")
    # The directory where the third-party libraries go to.
    SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
    # The directory where the auxiliary files go to.
    SET(OVITO_RELATIVE_SHARE_DIRECTORY "ovito")
    # The directory where the compiled plugins go to.
    SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
    # The directory where the Python source modules go to.
    SET(OVITO_RELATIVE_PYTHON_DIRECTORY ".")
ENDIF()

# The directory where the main executable goes to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_BINARY_DIRECTORY}")
    SET(OVITO_BINARY_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_BINARY_DIRECTORY}")
ELSE()
    SET(OVITO_BINARY_DIRECTORY "${OVITO_RELATIVE_BINARY_DIRECTORY}")
ENDIF()
# The directory where the main libraries go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
    SET(OVITO_LIBRARY_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
ELSE()
    SET(OVITO_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
ENDIF()
# The directory where the compiled plugins go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_PLUGINS_DIRECTORY}")
    SET(OVITO_PLUGINS_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}")
ELSE()
    SET(OVITO_PLUGINS_DIRECTORY "${OVITO_RELATIVE_PLUGINS_DIRECTORY}")
ENDIF()
# The directory where the Python source files go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_PYTHON_DIRECTORY}")
    SET(OVITO_PYTHON_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_PYTHON_DIRECTORY}")
ELSE()
    SET(OVITO_PYTHON_DIRECTORY "${OVITO_RELATIVE_PYTHON_DIRECTORY}")
ENDIF()
# The directory where the auxiliary files go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_SHARE_DIRECTORY}")
    SET(OVITO_SHARE_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_SHARE_DIRECTORY}")
ELSE()
    SET(OVITO_SHARE_DIRECTORY "${OVITO_RELATIVE_SHARE_DIRECTORY}")
ENDIF()

IF(APPLE OR WIN32 OR NOT OVITO_REDISTRIBUTABLE_PACKAGE)
    # Add the automatically determined parts of the RPATH,
    # which point to directories outside the build tree to the install RPATH.
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
ENDIF()

# Select linkage type (shared or static) for plugin libraries.
IF(OVITO_BUILD_MONOLITHIC)
    SET(BUILD_SHARED_LIBS OFF)
    SET(OVITO_PLUGIN_LIBRARY_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
ELSE()
    SET(BUILD_SHARED_LIBS ON)
    # Define name suffix used for generating plugin libraries.
    IF(APPLE)
        # On macOS, we use the .so extension instead of the standard .dylib to be compatible
        # with the Python interpreter, which only finds modules having a .so suffix.
        SET(OVITO_PLUGIN_LIBRARY_SUFFIX ".so")
    ELSE()
        SET(OVITO_PLUGIN_LIBRARY_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    ENDIF()
ENDIF()

# Disable link-time optimization (LTO) on Windows when using SYCL.
# Handles "lld-link: error: ... unknown file type"
IF(OVITO_USE_SYCL AND WIN32)
    SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
ENDIF()
IF(OVITO_USE_SYCL STREQUAL AdaptiveCpp AND NOT ADAPTIVECPP_DEBUG_LEVEL)
    IF(CMAKE_BUILD_TYPE MATCHES "Debug")
        SET(ADAPTIVECPP_DEBUG_LEVEL 3 CACHE INTEGER "Choose the debug level, options are: 0 (no debug), 1 (print errors), 2 (also print warnings), 3 (also print general information)" FORCE)
    ELSE()
        SET(ADAPTIVECPP_DEBUG_LEVEL 2 CACHE INTEGER "Choose the debug level, options are: 0 (no debug), 1 (print errors), 2 (also print warnings), 3 (also print general information)" FORCE)
    ENDIF()
ENDIF()

# Helper functions for finding and deploying third-party prerequisites.
INCLUDE(cmake/Prerequisites.cmake)

# Determine the command for running the clang-tidy tool.
IF(OVITO_RUN_CLANG_TIDY)
    FIND_PROGRAM(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
    SET(OVITO_CLANG_TIDY_CMD "${CLANG_TIDY_EXE}") # List of checks will be read from the .clang-tidy file.
ENDIF()

# Generate the build targets that are part of Ovito.
ADD_SUBDIRECTORY(src)

# This allows external CMake projects to inject their customization code into the CMake project:
IF(OVITO_CMAKE_FILE_INJECTION_PROJECT)
    INCLUDE("${OVITO_CMAKE_FILE_INJECTION_PROJECT}")
ENDIF()

# Generate the user manual.
ADD_SUBDIRECTORY(docs/manual)
