# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Build zstd compression library from source for static linking.
# Configure and build zstd using FetchContent. This provides cleaner integration without
# polluting the CMake namespace with zstd options.
INCLUDE(FetchContent)
SET(ZSTD_LEGACY_SUPPORT OFF)
SET(ZSTD_BUILD_PROGRAMS OFF)
SET(ZSTD_BUILD_TESTS OFF)
SET(ZSTD_BUILD_CONTRIB OFF)
SET(ZSTD_BUILD_STATIC ON)
SET(ZSTD_BUILD_SHARED OFF)

# Declare zstd using the local git submodule. Set EXCLUDE_FROM_ALL to avoid installation
# of zstd build artifacts together with the main OVITO build.
FetchContent_Declare(zstd SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/zstd/build/cmake EXCLUDE_FROM_ALL)

# Populate zstd (this processes its CMakeLists.txt with the options set above).
FetchContent_MakeAvailable(zstd)

# Create an alias for compatibility with existing code in core/CMakeLists.txt.
ADD_LIBRARY(zstd::libzstd_static ALIAS libzstd_static)

# Get the full version from the zstd sub-project.
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/zstd/build/cmake/CMakeModules")
INCLUDE(GetZstdLibraryVersion)
GetZstdLibraryVersion("zstd/lib/zstd.h" zstd_VERSION_MAJOR zstd_VERSION_MINOR zstd_VERSION_PATCH)
OVITO_REGISTER_PACKAGE_FOR_SBOM("zstd" "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")
