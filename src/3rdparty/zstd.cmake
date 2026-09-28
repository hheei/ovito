#######################################################################################
#
#  Copyright 2026 OVITO GmbH, Germany
#
#  This file is part of OVITO (Open Visualization Tool).
#
#  OVITO is free software; you can redistribute it and/or modify it either under the
#  terms of the GNU General Public License version 3 as published by the Free Software
#  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
#  If you do not alter this notice, a recipient may use your version of this
#  file under either the GPL or the MIT License.
#
#  You should have received a copy of the GPL along with this program in a
#  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
#  with this program in a file LICENSE.MIT.txt
#
#  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
#  either express or implied. See the GPL or the MIT License for the specific language
#  governing rights and limitations.
#
#######################################################################################

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
