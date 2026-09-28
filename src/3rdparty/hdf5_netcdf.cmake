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

# Build HDF5 and NetCDF-C from bundled git submodules for static linking.
# ExternalProject_Add is used (rather than FetchContent) so that HDF5 is fully built and
# *installed* to a local prefix (_ep/) before NetCDF-C's cmake configure step runs. This
# allows NetCDF-C's find_package(HDF5 CONFIG) to find a proper installed HDF5 package with
# real cmake IMPORTED targets — which FetchContent cannot provide, because its in-tree targets
# cannot be used in the try_compile() sub-projects that NetCDF-C's feature checks rely on.
INCLUDE(ExternalProject)
SET(_ep_prefix "${CMAKE_BINARY_DIR}/_ep")

# Forward macOS SDK / architecture settings into the EP sub-builds.
SET(_ep_platform_args)
IF(APPLE)
    IF(CMAKE_OSX_ARCHITECTURES)
        LIST(APPEND _ep_platform_args "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
    ENDIF()
    IF(CMAKE_OSX_DEPLOYMENT_TARGET)
        LIST(APPEND _ep_platform_args "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    ENDIF()
    IF(CMAKE_OSX_SYSROOT)
        LIST(APPEND _ep_platform_args "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")
    ENDIF()
ENDIF()

# Extract HDF5 version from source header and register in SBOM.
FILE(STRINGS "${CMAKE_CURRENT_LIST_DIR}/hdf5/src/H5public.h" _h5_major REGEX "^#define[ \t]+H5_VERS_MAJOR[ \t]+[0-9]")
FILE(STRINGS "${CMAKE_CURRENT_LIST_DIR}/hdf5/src/H5public.h" _h5_minor REGEX "^#define[ \t]+H5_VERS_MINOR[ \t]+[0-9]")
FILE(STRINGS "${CMAKE_CURRENT_LIST_DIR}/hdf5/src/H5public.h" _h5_release REGEX "^#define[ \t]+H5_VERS_RELEASE[ \t]+[0-9]")
STRING(REGEX REPLACE ".*[ \t]([0-9]+)[ \t]*$" "\\1" _h5_major "${_h5_major}")
STRING(REGEX REPLACE ".*[ \t]([0-9]+)[ \t]*$" "\\1" _h5_minor "${_h5_minor}")
STRING(REGEX REPLACE ".*[ \t]([0-9]+)[ \t]*$" "\\1" _h5_release "${_h5_release}")
SET(_h5_version "${_h5_major}.${_h5_minor}.${_h5_release}")
OVITO_REGISTER_PACKAGE_FOR_SBOM("HDF5" "${_h5_version}")

# ExternalProject stamp files record only that a step has run, not what it ran on. Since
# SOURCE_DIR points into a git submodule, moving that submodule to a different release
# would otherwise leave an existing build tree silently linking the previously built
# library: ninja reports "no work to do" and _ep/ keeps the old version. Making the stamp
# directory version-specific forces the sub-build to re-run whenever the bundled sources
# change version, while keeping incremental rebuilds free when they do not.
SET(_hdf5_stamp_dir "${CMAKE_BINARY_DIR}/_ep_build/hdf5-stamp-${_h5_version}")

# HDF5's cmake appends a debug postfix to static library names (_debug on macOS/Linux, _D on Windows).
# Compute the installed library paths before ExternalProject_Add since BUILD_BYPRODUCTS needs them.
IF(WIN32)
    SET(_h5_dbg_postfix "_D")
ELSE()
    SET(_h5_dbg_postfix "_debug")
ENDIF()
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" _build_type_lower)
IF(_build_type_lower STREQUAL "debug")
    SET(_h5_postfix "${_h5_dbg_postfix}")
ELSE()
    SET(_h5_postfix "")
ENDIF()
SET(_hdf5_lib    "${_ep_prefix}/lib/libhdf5${_h5_postfix}${CMAKE_STATIC_LIBRARY_SUFFIX}")
SET(_hdf5_hl_lib "${_ep_prefix}/lib/libhdf5_hl${_h5_postfix}${CMAKE_STATIC_LIBRARY_SUFFIX}")

# Probe for zlib now (before 3rdparty/ovito/core runs its own find). Zlib is optional in OVITO;
# enable HDF5 zlib compression only when zlib is available on this platform.
FIND_PACKAGE(ZLIB QUIET)
IF(ZLIB_FOUND)
    # Normalize to forward slashes; backslashes in paths passed as ExternalProject CMAKE_ARGS
    # end up in sub-project cmake string literals where \U etc. are invalid escape sequences.
    FILE(TO_CMAKE_PATH "${ZLIB_INCLUDE_DIR}" ZLIB_INCLUDE_DIR)
    FILE(TO_CMAKE_PATH "${ZLIB_LIBRARY}" ZLIB_LIBRARY)
    FILE(TO_CMAKE_PATH "${ZLIB_DIR}" ZLIB_DIR)
    SET(_hdf5_zlib_arg "-DHDF5_ENABLE_Z_LIB_SUPPORT=ON" "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}" "-DZLIB_LIBRARY=${ZLIB_LIBRARY}" "-DZLIB_DIR=${ZLIB_DIR}")
    SET(_netcdf_zlib_arg "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}" "-DZLIB_LIBRARY=${ZLIB_LIBRARY}")
ELSE()
    SET(_hdf5_zlib_arg "-DHDF5_ENABLE_Z_LIB_SUPPORT=OFF")
    SET(_netcdf_zlib_arg)
ENDIF()

# HDF5 external project: configure, build, and install into _ep/.
# CMAKE_C_VISIBILITY_PRESET=hidden hides all HDF5 symbols in the installed static lib so that
# they don't clash with the HDF5 copy loaded by h5py at runtime.
# BUILD_BYPRODUCTS is required by the Ninja generator to track that these .a files are
# produced by this ExternalProject, so it can order link steps correctly.
ExternalProject_Add(hdf5_ep
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/hdf5"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_ep_build/hdf5"
    STAMP_DIR "${_hdf5_stamp_dir}"
    INSTALL_DIR "${_ep_prefix}"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_C_VISIBILITY_PRESET=hidden
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=OFF
        -DHDF5_BUILD_TOOLS=OFF
        -DHDF5_BUILD_EXAMPLES=OFF
        -DBUILD_TESTING=OFF
        -DHDF5_BUILD_HL_LIB=ON
        ${_hdf5_zlib_arg}
        -DHDF5_ENABLE_SZIP_SUPPORT=OFF
        -DHDF5_BUILD_FORTRAN=OFF
        -DHDF5_BUILD_CPP_LIB=OFF
        -DHDF5_BUILD_JAVA=OFF
        ${_ep_platform_args}
    BUILD_BYPRODUCTS
        "${_hdf5_lib}"
        "${_hdf5_hl_lib}"
    EXCLUDE_FROM_ALL TRUE
)

# HDF5's installed hdf5-targets.cmake records the zlib dependency as "$<LINK_ONLY:ZLIB::ZLIB>"
# in hdf5-static's INTERFACE_LINK_LIBRARIES. cmake 4.x's try_compile IMPORTED-target exporter
# does not traverse generator expressions, so ZLIB::ZLIB is never written to the generated
# cmTC_...Targets.cmake for sub-projects. netcdf-c's check_symbol_exists() calls that set
# CMAKE_REQUIRED_LIBRARIES to HDF5::HDF5 then fail because ZLIB::ZLIB is undefined.
# Fix: right after hdf5_ep installs, rewrite the targets files to substitute the concrete zlib
# library path for the ZLIB::ZLIB target name. A plain path in $<LINK_ONLY:path> requires no
# target lookup and is handled correctly by try_compile.
IF(ZLIB_FOUND)
    SET(_hdf5_patch_script "${CMAKE_BINARY_DIR}/_ep_build/patch_hdf5_targets.cmake")
    FILE(WRITE "${_hdf5_patch_script}"
        "foreach(_f\n"
        "        \"${_ep_prefix}/cmake/hdf5-targets.cmake\"\n"
        "        \"${_ep_prefix}/cmake/hdf5-targets-release.cmake\"\n"
        "        \"${_ep_prefix}/cmake/hdf5-targets-debug.cmake\"\n"
        "        \"${_ep_prefix}/cmake/hdf5-targets-relwithdebinfo.cmake\")\n"
        "  if(EXISTS \"\${_f}\")\n"
        "    file(READ \"\${_f}\" _c)\n"
        "    string(REPLACE \"ZLIB::ZLIB\" \"${ZLIB_LIBRARY}\" _c \"\${_c}\")\n"
        "    file(WRITE \"\${_f}\" \"\${_c}\")\n"
        "  endif()\n"
        "endforeach()\n"
    )
    ExternalProject_Add_Step(hdf5_ep patch_zlib_target
        COMMAND ${CMAKE_COMMAND} -P "${_hdf5_patch_script}"
        DEPENDEES install
        ALWAYS FALSE
    )
ENDIF()

# Expose the HDF5 static libraries as INTERFACE targets. Using INTERFACE (not IMPORTED STATIC)
# ensures that add_dependencies() propagates the EP build ordering to all consumers.
# Both core (libhdf5) and HL (libhdf5_hl) are bundled into hdf5-static so that consumers
# (including libnetcdf, which references H5DS* HL symbols) get both libraries automatically.
# Zlib is added to the link chain only when it was found (and thus compiled into libhdf5).
ADD_LIBRARY(hdf5-static INTERFACE)
IF(ZLIB_FOUND)
    TARGET_LINK_LIBRARIES(hdf5-static INTERFACE "${_hdf5_hl_lib}" "${_hdf5_lib}" ZLIB::ZLIB)
ELSE()
    TARGET_LINK_LIBRARIES(hdf5-static INTERFACE "${_hdf5_hl_lib}" "${_hdf5_lib}")
ENDIF()
# H5system.c calls StrStrIA which lives in shlwapi.dll; propagate this dependency to all consumers.
IF(WIN32)
    TARGET_LINK_LIBRARIES(hdf5-static INTERFACE shlwapi)
ELSE()
    # HDF5's own CMakeLists.txt links ${CMAKE_DL_LIBS} into the hdf5 target on non-Windows
    # platforms (needed for dlopen/dlclose used by its dynamically loaded plugin support).
    # Since we link the installed static archive directly instead of going through HDF5's
    # exported imported target, that dependency must be added here explicitly, otherwise
    # dlclose ends up undefined at final link time on platforms where libdl is a separate DSO.
    TARGET_LINK_LIBRARIES(hdf5-static INTERFACE ${CMAKE_DL_LIBS})
ENDIF()
TARGET_INCLUDE_DIRECTORIES(hdf5-static INTERFACE "${_ep_prefix}/include")
ADD_DEPENDENCIES(hdf5-static hdf5_ep)

# hdf5_hl-static is a separate target for code that requests HL explicitly; it delegates to hdf5-static.
ADD_LIBRARY(hdf5_hl-static INTERFACE)
TARGET_LINK_LIBRARIES(hdf5_hl-static INTERFACE hdf5-static)

# Extract NetCDF version from source and register in SBOM.
FILE(STRINGS "${CMAKE_CURRENT_LIST_DIR}/netcdf-c/CMakeLists.txt" _nc_ver_line REGEX "^  VERSION [0-9]")
STRING(REGEX REPLACE "[ \t]*VERSION[ \t]+([0-9]+\\.[0-9]+\\.[0-9]+)" "\\1" _nc_version "${_nc_ver_line}")
OVITO_REGISTER_PACKAGE_FOR_SBOM("netCDF" "${_nc_version}")

# See the note above hdf5_ep's STAMP_DIR.
SET(_netcdf_stamp_dir "${CMAKE_BINARY_DIR}/_ep_build/netcdf-c-stamp-${_nc_version}")

# NetCDF-C external project: configure, build, and install into _ep/.
# CMAKE_PREFIX_PATH points to the EP prefix so that NetCDF-C's FindHDF5.cmake finds the
# HDF5 cmake config installed there by hdf5_ep.
# HDF5_USE_STATIC_LIBRARIES=ON is essential on Windows: without it, FindHDF5.cmake sets
# _suffix="-shared" when probing the config-mode targets, fails to find hdf5::hdf5-shared
# (we only have hdf5::hdf5-static), silently falls back to module-mode library search, and
# unconditionally adds -DH5_BUILT_AS_DYNAMIC_LIB on WIN32, making all H5_DLL declarations
# expand to __declspec(dllimport) — producing __imp_H5* references that can't be resolved
# from a static libhdf5.lib.
ExternalProject_Add(netcdf_ep
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/netcdf-c"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_ep_build/netcdf-c"
    STAMP_DIR "${_netcdf_stamp_dir}"
    INSTALL_DIR "${_ep_prefix}"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_C_VISIBILITY_PRESET=hidden
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
        -DHDF5_DIR="${_ep_prefix}/cmake"
        -DHDF5_USE_STATIC_LIBRARIES=ON
        -DBUILD_SHARED_LIBS=OFF
        -DNETCDF_ENABLE_DAP=OFF
        -DNETCDF_ENABLE_DAP2=OFF
        -DNETCDF_ENABLE_DAP4=OFF
        -DNETCDF_ENABLE_BYTERANGE=OFF
        -DNETCDF_ENABLE_TESTS=OFF
        -DNETCDF_BUILD_UTILITIES=OFF
        -DNETCDF_ENABLE_EXAMPLES=OFF
        -DNETCDF_ENABLE_HDF4=OFF
        -DNETCDF_ENABLE_PLUGINS=OFF
        -DNETCDF_ENABLE_NCZARR=OFF
        ${_netcdf_zlib_arg}
        ${_ep_platform_args}
    BUILD_BYPRODUCTS
        "${_ep_prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}netcdf${CMAKE_STATIC_LIBRARY_SUFFIX}"
    DEPENDS hdf5_ep
    EXCLUDE_FROM_ALL TRUE
)

ADD_LIBRARY(netcdf INTERFACE)
TARGET_LINK_LIBRARIES(netcdf INTERFACE "${_ep_prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}netcdf${CMAKE_STATIC_LIBRARY_SUFFIX}")
TARGET_INCLUDE_DIRECTORIES(netcdf INTERFACE "${_ep_prefix}/include")
ADD_DEPENDENCIES(netcdf netcdf_ep)
SET(netcdf_INCLUDE_DIRS "${_ep_prefix}/include")
