# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

###############################################################################
# The netcdf_integration CMake target provides a helper module that can be
# used by other modules in OVITO that make use of the NetCDF library. It
# pulls in the necessary header files and system libraries and provides the
# helper class NetCDFExclusiveAccess, which manages concurrent access to the
# NetCDF functions, which are not thread-safe.
###############################################################################

IF(NOT OVITO_REDISTRIBUTABLE_PACKAGE AND NOT OVITO_BUILD_PYPI)
    # In official release packages and PyPI wheels, HDF5 and NetCDF-C are built from bundled
    # git submodules (see src/3rdparty/CMakeLists.txt), providing in-tree static library targets
    # (hdf5-static, hdf5_hl-static, netcdf). In all other builds (conda, developer workstations,
    # distro system-package builds), locate HDF5 and NetCDF from the environment.
    OVITO_FIND_PACKAGE(HDF5 COMPONENTS C HL REQUIRED)
    OVITO_FIND_PACKAGE(NetCDF REQUIRED)
ENDIF()

IF(NOT TARGET netcdf AND NOT TARGET netCDF::netcdf)
    MESSAGE(FATAL_ERROR "NetCDF library target not found. Make sure HDF5 and NetCDF are available.")
ENDIF()

# Determine the type of library target to build.
SET(plugin_library_type "")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    IF(OVITO_BUILD_MONOLITHIC)
        SET(plugin_library_type "OBJECT")
    ELSEIF(BUILD_SHARED_LIBS)
        SET(plugin_library_type "MODULE")
    ENDIF()
ENDIF()

# Build library.
ADD_LIBRARY(NetCDFIntegration ${plugin_library_type} NetCDFIntegration.cpp)

# Give our library file a name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(NetCDFIntegration PROPERTIES OUTPUT_NAME "ovito_netcdf")

# Link to Qt.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)
TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC Qt6::Core)

# This library depends on the Core module of OVITO.
TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC Core)

# Inform source code that the NetCDF library is available.
TARGET_COMPILE_DEFINITIONS(NetCDFIntegration PUBLIC OVITO_NETCDF_SUPPORT)

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(NetCDFIntegration)

# Support SYCL.
OVITO_ADD_SYCL_TO_TARGET(NetCDFIntegration)

# Tell CMake to run Qt moc on source files added to the target.
SET_TARGET_PROPERTIES(NetCDFIntegration PROPERTIES AUTOMOC ON)
# Tell CMake to run the Qt resource compiler on all .qrc files added to a target.
SET_TARGET_PROPERTIES(NetCDFIntegration PROPERTIES AUTORCC ON)

# Define macro for symbol export from the shared library.
IF(BUILD_SHARED_LIBS)
    TARGET_COMPILE_DEFINITIONS(NetCDFIntegration PRIVATE "OVITO_NETCDF_INTEGRATION_EXPORT=Q_DECL_EXPORT")
    TARGET_COMPILE_DEFINITIONS(NetCDFIntegration INTERFACE "OVITO_NETCDF_INTEGRATION_EXPORT=Q_DECL_IMPORT")
ELSE()
    TARGET_COMPILE_DEFINITIONS(NetCDFIntegration PUBLIC "OVITO_NETCDF_INTEGRATION_EXPORT=")
ENDIF()

# Link the NetCDF library.
IF(TARGET netcdf)
    TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC netcdf)
    TARGET_INCLUDE_DIRECTORIES(NetCDFIntegration PUBLIC "${netcdf_INCLUDE_DIRS}")
ELSE()
    TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC netCDF::netcdf)
ENDIF()

# Expose the HDF5 C library (headers and link) to this module and its dependents, because the
# initializeHDF5Library() helper and OVITO's VASP HDF5 file reader use the HDF5 C API directly.
# This is required for every build flavour, including conda: HDF5 is only an indirect dependency
# of NetCDF, so linking the NetCDF target alone does not put HDF5 on our link line. On Windows
# that is fatal, because MSVC resolves each DLL's imports through that DLL's own import library.
# The imported target additionally carries H5_BUILT_AS_DYNAMIC_LIB when HDF5 is a shared library,
# which the HDF5 headers need in order to mark exported globals (H5T_NATIVE_*_g, H5_libinit_g,
# H5_libterm_g) as __declspec(dllimport). Without it those symbols stay unresolved at link time.
SET(_hdf5_is_static FALSE)
IF(TARGET hdf5-static)
    # In-tree static build from the bundled git submodule (release packages and PyPI wheels).
    TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC hdf5-static)
    SET(_hdf5_is_static TRUE)
ELSEIF(TARGET hdf5-shared)
    # Imported target defined by HDF5's own hdf5-config.cmake (this is what conda ships).
    TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC hdf5-shared)
ELSEIF(TARGET hdf5::hdf5-shared)
    TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC hdf5::hdf5-shared)
ELSEIF(TARGET hdf5::hdf5)
    # Imported target synthesized by CMake's FindHDF5 module.
    TARGET_LINK_LIBRARIES(NetCDFIntegration PUBLIC hdf5::hdf5)
ELSE()
    MESSAGE(FATAL_ERROR "HDF5 library target not found. Make sure HDF5 is available.")
ENDIF()

# When linking a shared HDF5 on Windows, the HDF5 headers must be told so, or they declare the
# library's exported globals without __declspec(dllimport) and the linker cannot resolve them.
# We set this ourselves rather than relying on the imported target to carry it: HDF5's own
# hdf5-config.cmake does put H5_BUILT_AS_DYNAMIC_LIB in INTERFACE_COMPILE_DEFINITIONS, but the
# equivalent branch in CMake's FindHDF5 module is dead code (it tests a variable the module never
# assigns), so the definition is missing whenever the module-mode target is the one we picked.
# Defining it a second time is harmless — CMake de-duplicates identical compile definitions.
IF(WIN32 AND NOT _hdf5_is_static)
    TARGET_COMPILE_DEFINITIONS(NetCDFIntegration PUBLIC H5_BUILT_AS_DYNAMIC_LIB)
ENDIF()

IF(OVITO_BUILD_CONDA AND WIN32)
    TARGET_COMPILE_DEFINITIONS(NetCDFIntegration PUBLIC DLL_NETCDF)
ENDIF()

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET NetCDFIntegration PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

# Export this target.
INSTALL(TARGETS NetCDFIntegration EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
