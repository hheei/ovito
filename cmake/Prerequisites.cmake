# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Qt modules that OVITO does not link against itself, but for which the bundled PySide6 package
# provides Python bindings. They must be deployed with the program package so that "import PySide6.QtXxx"
# works in the embedded Python interpreter. PrintSupport in particular is imported by the ipykernel
# Python module, which is needed to run the OVITO Pro Jupyter kernel.
# Note: This list is also consumed by cmake/FixupMacBundle.cmake, which deploys the corresponding
# Qt frameworks into the macOS app bundle.
SET(OVITO_PYSIDE_ONLY_QT_COMPONENTS PrintSupport)

# Determines the SONAME of a shared library, which is required
# to install the library in the OVITO program directory.
# The SONAME is the name of the library file without the full version number.
# This function is only available on Unix/Linux based platforms.
FUNCTION(get_library_soname OUTPUT_VAR LIBRARY_FILE)

    # Use the objdump command to read out the SONAME of the shared library.
    EXECUTE_PROCESS(COMMAND objdump -p "${LIBRARY_FILE}" COMMAND grep "SONAME" OUTPUT_VARIABLE _output_var OUTPUT_STRIP_TRAILING_WHITESPACE)
    STRING(REPLACE "SONAME" "" lib_soname "${_output_var}")
    STRING(STRIP "${lib_soname}" lib_soname)
    IF(NOT lib_soname)

        IF(APPLE)
            # On macOS platform, fall back to using the otool to determine the install name of the dyld library.
            EXECUTE_PROCESS(COMMAND otool -D "${LIBRARY_FILE}" OUTPUT_VARIABLE _output_var OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
            # _output_var contains a two lines: The first line is the file path of the library, the second line is the install name.
            # Extract the second line.
            STRING(REGEX REPLACE "\n" ";" _output_var "${_output_var}")
            LIST(GET _output_var 1 _output_var)
            # If an absolute path is returned, extract the file name from it.
            GET_FILENAME_COMPONENT(lib_soname "${_output_var}" NAME)
        ENDIF()

        IF(NOT lib_soname)
            MESSAGE(FATAL_ERROR "Failed to determine SONAME of shared library: ${LIBRARY_FILE}")
        ENDIF()
    ENDIF()

    SET(${OUTPUT_VAR} ${lib_soname} PARENT_SCOPE)

ENDFUNCTION()

# This function installs a third-party shared library/DLL in the OVITO program directory
# so that it can be distributed together with the program.
# On Unix/Linux based platforms it takes care of installing symbolic links as well.
# This macro creates an OVITO plugin module.
MACRO(OVITO_INSTALL_SHARED_LIB shared_lib)

    # Parse macro arguments.
    CMAKE_PARSE_ARGUMENTS(ARG
        "OPTIONAL;EXACT" # options
        "DESTINATION"  # one-value keywords
        "" # multi-value keywords
        ${ARGN}) # strings to parse

    # Validate argument values.
    IF(ARG_UNPARSED_ARGUMENTS)
        MESSAGE(FATAL_ERROR "Bad macro arguments: ${ARG_UNPARSED_ARGUMENTS}")
    ENDIF()
    SET(destination_dir ${ARG_DESTINATION})

    # Install libs in third-party library directory by default.
    IF(NOT destination_dir)
        SET(destination_dir ".")
    ENDIF()

    IF(WIN32 OR OVITO_REDISTRIBUTABLE_PACKAGE OR OVITO_BUILD_PYPI)
        # Replace backslashes in the path with regular slashes.
        STRING(REGEX REPLACE "\\\\" "/" _shared_lib ${shared_lib})
        # Make sure the destination directory exists.
        SET(_abs_dest_dir "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}")
        FILE(MAKE_DIRECTORY "${_abs_dest_dir}")
        # Strip version number from shared lib filename.
        GET_FILENAME_COMPONENT(shared_lib_ext "${_shared_lib}" EXT)
        STRING(REPLACE ${shared_lib_ext} "" lib_base_name "${_shared_lib}")

        # Find all files/symlinks in the same directory having the same base name.
        #
        # EXACT suppresses that search and takes only the file named. The sibling search exists
        # so that a package gets the whole symlink chain of a library, but it also collects
        # unrelated soversions that happen to sit in the same directory: asking for
        # libcrypto.so.1.1, which the Kerberos libraries need, also picked up the 7 MB
        # libcrypto.so.3 next to it and put it in every wheel.
        IF(ARG_EXACT)
            SET(lib_versions "${_shared_lib}")
        ELSE()
            FILE(GLOB lib_versions LIST_DIRECTORIES FALSE "${_shared_lib}" "${lib_base_name}.*${CMAKE_SHARED_LIBRARY_SUFFIX}" "${lib_base_name}${CMAKE_SHARED_LIBRARY_SUFFIX}.*")
        ENDIF()
        IF(NOT lib_versions)
            IF(${ARG_OPTIONAL})
                MESSAGE(STATUS "Did not find any library files matching the file path ${_shared_lib} --> skipping installation because this lib is optional")
            ELSE()
                MESSAGE(FATAL_ERROR "Did not find any library files that match the file path ${_shared_lib} (globbing patterns: ${lib_base_name}.*${CMAKE_SHARED_LIBRARY_SUFFIX}; ${lib_base_name}${CMAKE_SHARED_LIBRARY_SUFFIX}.*)")
            ENDIF()
        ENDIF()

        # Find all variants of the shared library name, including symbolic links.
        UNSET(lib_files)
        FOREACH(lib_version ${lib_versions})
            WHILE(IS_SYMLINK ${lib_version})
                GET_FILENAME_COMPONENT(symlink_target "${lib_version}" REALPATH)
                GET_FILENAME_COMPONENT(symlink_target_name "${symlink_target}" NAME)
                GET_FILENAME_COMPONENT(lib_version_name "${lib_version}" NAME)
                IF(NOT lib_version_name STREQUAL symlink_target_name AND NOT OVITO_BUILD_PYPI)
                    MESSAGE("Installing symlink ${lib_version_name} to ${symlink_target_name}")
                    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink ${symlink_target_name} "${_abs_dest_dir}/${lib_version_name}" COMMAND_ERROR_IS_FATAL ANY)
                    IF(NOT APPLE)
                        INSTALL(FILES "${_abs_dest_dir}/${lib_version_name}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
                    ENDIF()
                ENDIF()
                SET(lib_version "${symlink_target}")
            ENDWHILE()
            IF(NOT IS_SYMLINK ${lib_version})
                LIST(APPEND lib_files "${lib_version}")
            ENDIF()
        ENDFOREACH()
        LIST(REMOVE_DUPLICATES lib_files)

        FOREACH(lib_file ${lib_files})
            MESSAGE("Installing shared library ${lib_file}")
            EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${lib_file}" "${_abs_dest_dir}/" COMMAND_ERROR_IS_FATAL ANY)
            IF(WIN32 OR NOT OVITO_BUILD_PYPI)
                INSTALL(FILES "${lib_file}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
            ELSE()
                # Detect if this .so file is a linker script starting with the string "INPUT".
                # The TBB libraries use this special GNU ld feature instead of regular symbolic links to create aliases of a shared library in the same directory.
                FILE(READ "${lib_file}" _SO_FILE_HEADER LIMIT 5 HEX)
                IF("${_SO_FILE_HEADER}" STREQUAL "494e505554") # 0x494e505554 = "INPUT"
                    INSTALL(FILES "${lib_file}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
                ELSE()
                    # When building a Python wheel, we need to rename the shared library to its SONAME.
                    # That's because the Python wheel format does not support symbolic links.
                    # Use the objdump command to read out the SONAME of the shared library.
                    get_library_soname(lib_soname "${lib_file}")
                    GET_FILENAME_COMPONENT(lib_filename "${lib_file}" NAME)
                    FILE(RENAME "${_abs_dest_dir}/${lib_filename}" "${_abs_dest_dir}/${lib_soname}")
                    INSTALL(PROGRAMS "${_abs_dest_dir}/${lib_soname}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
                ENDIF()
            ENDIF()
        ENDFOREACH()
        UNSET(lib_files)
    ENDIF()
ENDMACRO()

# Reads the SONAMEs of the direct dependencies (DT_NEEDED entries) of an ELF file, keeping only
# those matching the given regular expression. Used to discover which system libraries a
# prebuilt binary actually links against, instead of hard-coding file names that vary between
# the Linux distributions OVITO is built on.
FUNCTION(OVITO_GET_ELF_DEPENDENCIES output_var elf_file name_regex)
    SET(_objdump "${CMAKE_OBJDUMP}")
    IF(NOT _objdump)
        SET(_objdump "objdump")
    ENDIF()
    EXECUTE_PROCESS(COMMAND "${_objdump}" -p "${elf_file}"
        OUTPUT_VARIABLE _dynamic_section
        ERROR_VARIABLE _objdump_error
        RESULT_VARIABLE _objdump_result
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    IF(NOT _objdump_result EQUAL 0)
        MESSAGE(FATAL_ERROR "Failed to read the dynamic section of ${elf_file} using '${_objdump}': ${_objdump_error}")
    ENDIF()
    SET(_sonames "")
    STRING(REGEX MATCHALL "NEEDED[ \t]+[^ \t\r\n]+" _needed_entries "${_dynamic_section}")
    FOREACH(_entry IN LISTS _needed_entries)
        STRING(REGEX REPLACE "^NEEDED[ \t]+" "" _soname "${_entry}")
        IF("${_soname}" MATCHES "${name_regex}")
            LIST(APPEND _sonames "${_soname}")
        ENDIF()
    ENDFOREACH()
    SET(${output_var} "${_sonames}" PARENT_SCOPE)
ENDFUNCTION()

# Helper function that recursively gathers a list of libraries and other targets that the given
# target depends on directly and indirectly.
FUNCTION(OVITO_GET_ALL_TARGET_DEPENDENCIES OUTPUT_LIST TARGET)

    # This special handling was adopted from __qt_internal_walk_libs() to avoid an error produced by older CMake versions:
    IF(${TARGET} STREQUAL "Qt6::EntryPoint")
        # We can't (and don't need to) process EntryPoint because it brings in $<TARGET_PROPERTY:prop>
        # genexes which get replaced with $<TARGET_PROPERTY:EntryPoint,prop> genexes in the code below
        # and that causes 'INTERFACE_LIBRARY targets may only have whitelisted properties.' errors
        # with CMake versions equal to or lower than 3.18. These errors are super unintuitive to
        # debug because there's no mention that it's happening during a file(GENERATE) call.
        RETURN()
    ENDIF()

    # Skip the following targets, because they cause problems when querying the LINK_LIBRARIES property below.
    IF(${TARGET} MATCHES "pybind11" OR ${TARGET} STREQUAL "documentation" OR ${TARGET} STREQUAL "scripting_documentation" OR ${TARGET} STREQUAL "scripting_documentation_prerun")
        RETURN()
    ENDIF()

    GET_TARGET_PROPERTY(IMPORTED ${TARGET} IMPORTED)
    IF(IMPORTED)
        GET_TARGET_PROPERTY(LIBS ${TARGET} INTERFACE_LINK_LIBRARIES)
    ELSE()
        GET_TARGET_PROPERTY(LIBS ${TARGET} LINK_LIBRARIES)
    ENDIF()
    GET_TARGET_PROPERTY(DEPENDENCIES ${TARGET} MANUALLY_ADDED_DEPENDENCIES)
    LIST(APPEND LIBS ${DEPENDENCIES})
    FOREACH(LIB ${LIBS})
        IF(NOT LIB IN_LIST ${OUTPUT_LIST})
            LIST(APPEND ${OUTPUT_LIST} ${LIB})
            IF(TARGET ${LIB})
                OVITO_GET_ALL_TARGET_DEPENDENCIES(${OUTPUT_LIST} ${LIB})
            ENDIF()
        ENDIF()
    ENDFOREACH()
    SET(${OUTPUT_LIST} ${${OUTPUT_LIST}} PARENT_SCOPE)
ENDFUNCTION()

# This function deploys the required Qt libraries with the program package.
FUNCTION(OVITO_DEPLOY_QT_FRAMEWORK_FILES)

    # Gather all indirect dependencies of the main executable including all plugins.
    IF(OVITO_BUILD_APP)
        OVITO_GET_ALL_TARGET_DEPENDENCIES(ovito_dependency_libraries Ovito)
    ENDIF()
    # When building just the Python bindings, get the dependencies from this target.
    IF(TARGET ovito_bindings)
        OVITO_GET_ALL_TARGET_DEPENDENCIES(ovito_dependency_libraries ovito_bindings)
    ENDIF()

    # Filter dependency list to find all Qt framework modules (targets starting with Qt6::).
    FOREACH(lib ${ovito_dependency_libraries})
        IF(lib MATCHES "^Qt6::(.+)")
            LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS ${CMAKE_MATCH_1})
        ENDIF()
    ENDFOREACH()

    # Amend Qt modules list with indirect dependencies.
    # DBus module is an indirect dependency of the Xcb platform plugin under Linux.
    IF(NOT "DBus" IN_LIST OVITO_REQUIRED_QT_COMPONENTS)
        LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS DBus)
    ENDIF()
    # Svg module is an indirect dependency of the SVG icon engine plugin.
    IF(NOT "Svg" IN_LIST OVITO_REQUIRED_QT_COMPONENTS)
        LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS Svg)
    ENDIF()
    # These modules are not used by OVITO itself, but the bundled PySide6 package provides Python bindings for them.
    IF(OVITO_BUILD_PROFESSIONAL)
        FOREACH(component IN LISTS OVITO_PYSIDE_ONLY_QT_COMPONENTS)
            IF(NOT component IN_LIST OVITO_REQUIRED_QT_COMPONENTS)
                LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS ${component})
            ENDIF()
        ENDFOREACH()
    ENDIF()

    SET(QT_NO_PRIVATE_MODULE_WARNING ON) # Suppress warning about Qt::GuiPrivate being a module with private APIs.

    # Pull in the Qt modules as CMake targets.
    FOREACH(qtmodule IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
        OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS ${qtmodule} REQUIRED)
    ENDFOREACH()

    # Search path for the system libraries that get bundled with OVITO.
    #
    # /usr/local/lib comes first because that is where the release image installs the
    # libraries it builds from source, and those have to win over the older copies of
    # the distribution. Note that CMake rewrites each of these paths, searching the
    # lib64 variant of a lib directory *before* the directory itself, once a compiler
    # has been detected. Listing /usr/lib first therefore made a plain FIND_LIBRARY
    # resolve to /usr/lib64, and OVITO shipped the fontconfig of the base image rather
    # than the one built for it.
    SET(OVITO_SYSTEM_LIBRARY_SEARCH_PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib64)

    IF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)

        # Install copies of the Qt libraries.
        FILE(MAKE_DIRECTORY "${OVITO_LIBRARY_DIRECTORY}/lib")
        FOREACH(component IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
            IF(component STREQUAL "GuiPrivate")
                CONTINUE() # Skip the private Qt Gui module, which is not a real module and does not have a corresponding shared lib.
            ENDIF()
            GET_TARGET_PROPERTY(lib Qt6::${component} LOCATION)
            GET_TARGET_PROPERTY(lib_soname Qt6::${component} IMPORTED_SONAME_RELEASE)
            CONFIGURE_FILE("${lib}" "${OVITO_LIBRARY_DIRECTORY}" COPYONLY)
            GET_FILENAME_COMPONENT(lib_realname "${lib}" NAME)
            EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${lib_realname}" "${OVITO_LIBRARY_DIRECTORY}/${lib_soname}" COMMAND_ERROR_IS_FATAL ANY)
            EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "../${lib_soname}" "${OVITO_LIBRARY_DIRECTORY}/lib/${lib_soname}" COMMAND_ERROR_IS_FATAL ANY)
            INSTALL(FILES "${lib}" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/")
            INSTALL(FILES "${OVITO_LIBRARY_DIRECTORY}/${lib_soname}" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/")
            INSTALL(FILES "${OVITO_LIBRARY_DIRECTORY}/lib/${lib_soname}" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/lib/")
            GET_FILENAME_COMPONENT(QtBinaryPath ${lib} PATH)
        ENDFOREACH()

        # Install Qt plugins.
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platforms/libqminimal.so" DESTINATION "./plugins_qt/platforms")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platforms/libqoffscreen.so" DESTINATION "./plugins_qt/platforms")
        # The ovitoheadless QPA plugin is built by OVITO itself; it is installed via its own CMakeLists.txt.
        # No OVITO_INSTALL_SHARED_LIB call is needed here because INSTALL(TARGETS ...) covers it.
        # Qt 6.10 merged the former libqwayland-generic.so and libqwayland-egl.so into a single
        # platform plugin, which provides all Wayland plugin keys ("wayland", "wayland-egl").
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platforms/libqwayland.so" DESTINATION "./plugins_qt/platforms")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platforms/libqxcb.so" DESTINATION "./plugins_qt/platforms")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/wayland-shell-integration/libqt-shell.so" DESTINATION "./plugins_qt/wayland-shell-integration")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/wayland-shell-integration/libwl-shell-plugin.so" DESTINATION "./plugins_qt/wayland-shell-integration")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/wayland-shell-integration/libxdg-shell.so" DESTINATION "./plugins_qt/wayland-shell-integration")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/wayland-shell-integration/libivi-shell.so" DESTINATION "./plugins_qt/wayland-shell-integration")
        # Note: the wayland-graphics-integration-client plugins are EGL/OpenGL buffer sharing
        # backends. Qt does not build them in our OpenGL-less configuration, and OVITO does not
        # need them, because it renders through Vulkan.
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/wayland-decoration-client/libadwaita.so" DESTINATION "./plugins_qt/wayland-decoration-client")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/wayland-decoration-client/libbradient.so" DESTINATION "./plugins_qt/wayland-decoration-client")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqgif.so" DESTINATION "./plugins_qt/imageformats")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqico.so" DESTINATION "./plugins_qt/imageformats" OPTIONAL)
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqicns.so" DESTINATION "./plugins_qt/imageformats" OPTIONAL)
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqjpeg.so" DESTINATION "./plugins_qt/imageformats")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqwebp.so" DESTINATION "./plugins_qt/imageformats")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqtiff.so" DESTINATION "./plugins_qt/imageformats")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/libqsvg.so" DESTINATION "./plugins_qt/imageformats")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/iconengines/libqsvgicon.so" DESTINATION "./plugins_qt/iconengines")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platformthemes/libqxdgdesktopportal.so" DESTINATION "./plugins_qt/platformthemes")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/networkinformation/libqnetworkmanager.so" DESTINATION "./plugins_qt/networkinformation")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/networkinformation/libqglib.so" DESTINATION "./plugins_qt/networkinformation" OPTIONAL)
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/libqcertonlybackend.so" DESTINATION "./plugins_qt/tls" OPTIONAL)
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/libqopensslbackend.so" DESTINATION "./plugins_qt/tls")
        # libQt6XcbQpa.so is required by the Qt Gui module and XCB platform plugin.
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/libQt6XcbQpa.so" DESTINATION "./lib")
        # libQt6WaylandClient.so and others are required by the Qt platform plugin libqwayland.so and its sub-plugins.
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/libQt6WaylandClient.so" DESTINATION "./lib")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/libQt6WlShellIntegration.so" DESTINATION "./lib")

        # Distribute libxkbcommon.so with OVITO, which is a dependency of the Qt XCB plugin that might not be present on all systems.
        FIND_LIBRARY(OVITO_XKBCOMMON_DEP NAMES libxkbcommon.so.0 PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
        OVITO_INSTALL_SHARED_LIB("${OVITO_XKBCOMMON_DEP}" DESTINATION "./lib")
        UNSET(OVITO_XKBCOMMON_DEP CACHE)
        # Additionally, place a symlink into the parent lib/ovito/ directory.
        EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "lib/libxkbcommon.so.0" "${OVITO_LIBRARY_DIRECTORY}/libxkbcommon.so.0" COMMAND_ERROR_IS_FATAL ANY)
        INSTALL(FILES "${OVITO_LIBRARY_DIRECTORY}/libxkbcommon.so.0" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/")

        # Distribute libxkbcommon-x11.so with OVITO, which is a dependency of the Qt XCB plugin that might not be present on all systems.
        FIND_LIBRARY(OVITO_XKBCOMMONX11_DEP NAMES libxkbcommon-x11.so.0 PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
        OVITO_INSTALL_SHARED_LIB("${OVITO_XKBCOMMONX11_DEP}" DESTINATION "./lib")
        UNSET(OVITO_XKBCOMMONX11_DEP CACHE)

        # Distribute the xcb-util family of libraries, which the Qt XCB platform plugin
        # links against. Unlike the core X client libraries, these live in six separate
        # packages that minimal installations commonly lack, and xcb-util-cursor is not
        # even part of the base repositories of RHEL 8 and its derivatives - which is the
        # single most frequent reason for the graphical user interface not starting.
        #
        # They are safe to ship: together they measure some 120 KB, they are protocol
        # helpers without any compiled-in data paths, and everything they need below them
        # comes from libxcb itself, whose symbols are unversioned and only ever added to.
        # OVITO therefore bundles these consumers while taking the provider from the
        # system. The core X client libraries are deliberately not bundled: libX11 has the
        # locale, keysym and error database paths compiled into it, and it has to match
        # the X server of the machine anyway.
        #
        # They go next to libxkbcommon-x11.so, because that directory is what the runpath
        # of the platform plugin, $ORIGIN/../../lib, already points at.
        FOREACH(_xcblib libxcb-cursor.so.0 libxcb-icccm.so.4 libxcb-image.so.0 libxcb-keysyms.so.1 libxcb-render-util.so.0 libxcb-util.so.1)
            FIND_LIBRARY(OVITO_XCBUTIL_DEP NAMES ${_xcblib} PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
            OVITO_INSTALL_SHARED_LIB("${OVITO_XCBUTIL_DEP}" DESTINATION "./lib")
            UNSET(OVITO_XCBUTIL_DEP CACHE)
        ENDFOREACH()

        # libxcb-xinerama.so used to be shipped here as a dependency of the Qt XCB platform
        # plugin. Qt 6 reads the screen layout through XRandR and dropped Xinerama entirely:
        # no library or plugin of either Qt build references it, and nothing in the package
        # did either, so it was two megabytes that could never be loaded.

        # Distribute the fontconfig/freetype stack with OVITO. The Qt Gui module links against
        # these two libraries directly, and after configuring Qt with -no-opengl, -no-glib and
        # -DFEATURE_dbus_linked=OFF they are the only external system libraries that the Qt build
        # still requires. Shipping them makes the package start on bare systems that have no desktop
        # or development packages installed at all, for example HPC compute nodes.
        FOREACH(_fontlib libfontconfig.so.1 libfreetype.so.6 libexpat.so.1 libpng16.so.16 libbz2.so.1 libz.so.1 libuuid.so.1)
            FIND_LIBRARY(OVITO_FONT_DEP NAMES ${_fontlib} PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
            OVITO_INSTALL_SHARED_LIB("${OVITO_FONT_DEP}" DESTINATION ".")
            UNSET(OVITO_FONT_DEP CACHE)
        ENDFOREACH()

        # Distribute the ICU libraries that this Qt build links against, if any.
        #
        # ICU raises its SONAME with every release, so the file names depend on the distribution
        # Qt was compiled on and cannot be hard-coded. Read the names out of the dynamic section
        # of the Qt modules themselves, which is authoritative, and follow ICU's own internal
        # dependencies. A Qt configured with -no-icu contributes no entries, and nothing is
        # installed.
        SET(_icu_pending "")
        FOREACH(component IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
            IF(component STREQUAL "GuiPrivate")
                CONTINUE()
            ENDIF()
            GET_TARGET_PROPERTY(_qt_module_lib Qt6::${component} LOCATION)
            OVITO_GET_ELF_DEPENDENCIES(_module_icu_deps "${_qt_module_lib}" "^libicu")
            LIST(APPEND _icu_pending ${_module_icu_deps})
        ENDFOREACH()

        SET(_icu_resolved "")
        WHILE(_icu_pending)
            LIST(POP_FRONT _icu_pending _icu_soname)
            IF("${_icu_soname}" IN_LIST _icu_resolved)
                CONTINUE()
            ENDIF()
            LIST(APPEND _icu_resolved "${_icu_soname}")
            FIND_LIBRARY(OVITO_ICU_DEP NAMES "${_icu_soname}"
                PATHS ${QtBinaryPath} /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib64
                NO_DEFAULT_PATH REQUIRED)
            OVITO_INSTALL_SHARED_LIB("${OVITO_ICU_DEP}" DESTINATION ".")
            # The ICU libraries depend on one another, so pull in their dependencies as well.
            OVITO_GET_ELF_DEPENDENCIES(_nested_icu_deps "${OVITO_ICU_DEP}" "^libicu")
            LIST(APPEND _icu_pending ${_nested_icu_deps})
            UNSET(OVITO_ICU_DEP CACHE)
        ENDWHILE()
        IF(_icu_resolved)
            MESSAGE(STATUS "Bundling ICU libraries required by Qt: ${_icu_resolved}")
        ENDIF()

        # Ship Mesa's software Vulkan driver (lavapipe) together with a Vulkan loader as a
        # last-resort fallback for systems that provide no Vulkan driver at all. The directory
        # is staged by the release build image (see ci/docker/linux_release_build/Dockerfile).
        #
        # It must NOT be reachable through the runpath of any other shipped object: the bundled
        # loader may only ever be used on a machine that has none of its own, and must never
        # shadow a working GPU driver. RenderThread::prepareVulkanEnvironment() decides at
        # runtime whether to activate it.
        IF(EXISTS "${OVITO_VULKAN_FALLBACK_DIR}/lvp_icd.json")
            INSTALL(DIRECTORY "${OVITO_VULKAN_FALLBACK_DIR}/"
                DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/vulkan-fallback"
                USE_SOURCE_PERMISSIONS)
        ELSE()
            MESSAGE(WARNING "No software Vulkan fallback found in ${OVITO_VULKAN_FALLBACK_DIR}. "
                "The package will require the target system to provide a Vulkan driver. "
                "Set OVITO_VULKAN_FALLBACK_DIR to the directory staged by the release build image.")
        ENDIF()

    ELSEIF(LINUX AND OVITO_BUILD_PYPI)

        # Bundle the system libraries that the PyPI wheel needs, so that "import ovito" works on
        # a machine where the user cannot install anything.
        #
        # The wheel ships no Qt of its own: ovito_bindings.so loads the Qt libraries of the
        # PySide6 package it depends on, and those are the Qt Company's build, with OpenGL, GLib
        # and D-Bus switched on. Merely importing the module therefore pulls in fourteen system
        # libraries, seven to ten of which are absent from a freshly installed distribution --
        # measured on ubuntu 22.04/24.04, debian 12, rocky 9, almalinux 8, opensuse leap 15.6 and
        # fedora 41. None of the work that made the program package self-contained helps here,
        # because none of it reaches the Qt that PySide6 ships.
        #
        # They go into a *subdirectory* of the plugins directory, which is the point: $ORIGIN of
        # ovito_bindings.so is the plugins directory itself, so a copy of libGL.so.1 placed next
        # to it would satisfy the dynamic linker unconditionally and shadow a perfectly good
        # driver on a machine that has one. Nothing reaches into "syslibs/" through a runpath.
        # ovito/plugins/__init__.py loads from it explicitly, and only for those SONAMEs the host
        # cannot resolve itself. This is the same arrangement that keeps the software Vulkan
        # driver of the program package off every search path.
        #
        # The list is written out rather than derived from the Qt this is compiled against. That
        # Qt lives in the "qt6-full" prefix and is configured to match PySide6's feature set, but
        # it is not the same binary: ours declares libGLX/libOpenGL where the Qt Company's
        # declares libGL.so.1. What keeps the list honest is the linux_wheel_against_pyside6 CI
        # job, which imports the wheel on an image where nothing is installed.
        SET(OVITO_PYPI_SYSTEM_LIBS
            # libglvnd. OVITO calls no OpenGL function -- it renders through Vulkan, and the
            # program package has no GL dependency at all -- but PySide6's libQt6Gui links
            # libGL.so.1 and the linker resolves that before any OVITO code runs. These five
            # belong together: libEGL and libGLX reach libGLdispatch through a private,
            # unversioned interface, and only one libGLdispatch exists per process.
            libGL.so.1 libEGL.so.1 libGLX.so.0 libGLdispatch.so.0 libOpenGL.so.0
            # Font rendering. Same set the program package bundles.
            libfontconfig.so.1 libfreetype.so.6 libexpat.so.1 libpng16.so.16 libbz2.so.1
            libz.so.1 libuuid.so.1
            # X client libraries. The program package deliberately leaves these to the system,
            # because libX11 has its locale, keysym and error database paths compiled in and has
            # to match the X server. Here there is no choice: libQt6Gui names libX11.so.6 in its
            # DT_NEEDED even when nothing is ever displayed. A machine with a display has its own
            # copy and keeps using it, since the loader is only asked for what it cannot find.
            libX11.so.6 libX11-xcb.so.1 libxcb.so.1 libXau.so.6 libXext.so.6
            # Keyboard handling.
            libxkbcommon.so.0 libxkbcommon-x11.so.0
            # GLib, built from source in the release image; see the Dockerfile for why the copy
            # in the base image is unusable here. That build links PCRE2 statically, so these
            # two files need nothing below them but glibc.
            libglib-2.0.so.0 libgthread-2.0.so.0
            # D-Bus. Linked by libQt6DBus, which libQt6Gui pulls in.
            libdbus-1.so.3
            # Kerberos, linked by libQt6Network. The whole chain is dead weight in practice --
            # Qt uses GSSAPI only for SPNEGO proxy authentication, which OVITO never performs --
            # but it sits in libQt6Network's DT_NEEDED, so the dynamic linker insists on it
            # before any code runs. The OpenSSL it needs is added below rather than named here.
            libgssapi_krb5.so.2 libkrb5.so.3 libk5crypto.so.3 libkrb5support.so.0
            libcom_err.so.2 libkeyutils.so.1
            # Compression, linked by libQt6Core and libQt6Network.
            libbrotlidec.so.1 libbrotlicommon.so.1 libzstd.so.1
            # The xcb-util family, needed by PySide6's XCB platform plugin when a script uses
            # ovito.gui on a machine that does have a display. These live in six separate
            # packages that minimal installations commonly lack, and xcb-util-cursor is not in
            # the base repositories of RHEL 8 at all.
            libxcb-cursor.so.0 libxcb-icccm.so.4 libxcb-image.so.0 libxcb-keysyms.so.1
            libxcb-render-util.so.0 libxcb-util.so.1
        )
        FOREACH(_syslib IN LISTS OVITO_PYPI_SYSTEM_LIBS)
            FIND_LIBRARY(OVITO_PYPI_SYSLIB_DEP NAMES ${_syslib} PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
            OVITO_INSTALL_SHARED_LIB("${OVITO_PYPI_SYSLIB_DEP}" DESTINATION "syslibs")
            UNSET(OVITO_PYPI_SYSLIB_DEP CACHE)
        ENDFOREACH()

        # The OpenSSL that the Kerberos libraries were built against. Which soversion that is
        # depends on the base image -- AlmaLinux 8 on x86_64 names libcrypto.so.1.1, RHEL 10 on
        # aarch64 names libcrypto.so.3 -- so read it out of the library instead of hard-coding
        # either one, which made the aarch64 build fail outright. Installed EXACTly, so that the
        # other soversion sitting in the same directory does not come along for the ride.
        # Nothing ever calls into it; OVITO's own TLS uses a separate, current OpenSSL.
        FIND_LIBRARY(OVITO_KRB5_LIB NAMES libgssapi_krb5.so.2 PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
        OVITO_GET_ELF_DEPENDENCIES(_krb5_crypto_sonames "${OVITO_KRB5_LIB}" "^libcrypto\\.so\\.")
        UNSET(OVITO_KRB5_LIB CACHE)
        FOREACH(_soname IN LISTS _krb5_crypto_sonames)
            FIND_LIBRARY(OVITO_PYPI_CRYPTO_DEP NAMES ${_soname} PATHS ${OVITO_SYSTEM_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH REQUIRED)
            GET_FILENAME_COMPONENT(OVITO_PYPI_CRYPTO_DEP "${OVITO_PYPI_CRYPTO_DEP}" REALPATH)
            MESSAGE(STATUS "Bundling the OpenSSL required by Kerberos: ${_soname} (${OVITO_PYPI_CRYPTO_DEP})")
            OVITO_INSTALL_SHARED_LIB("${OVITO_PYPI_CRYPTO_DEP}" DESTINATION "syslibs" EXACT)
            UNSET(OVITO_PYPI_CRYPTO_DEP CACHE)
        ENDFOREACH()

    ELSEIF(WIN32 AND NOT OVITO_BUILD_PYPI AND NOT OVITO_BUILD_CONDA)

        # On Windows, the third-party library DLLs need to be installed in the OVITO directory.
        # Gather Qt dynamic link libraries.
        FOREACH(component IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
            IF(component STREQUAL "GuiPrivate")
                CONTINUE() # Skip the private Qt Gui module, which is not a real module and does not have a corresponding DLL.
            ENDIF()
            GET_TARGET_PROPERTY(dll Qt6::${component} LOCATION_${CMAKE_BUILD_TYPE})
            IF(NOT TARGET Qt6::${component})
                MESSAGE(FATAL_ERROR "Target does not exist: Qt6::${component}")
            ENDIF()
            IF(NOT dll)
                MESSAGE(FATAL_ERROR "Target has no LOCATION property: Qt6::${component}")
            ENDIF()
            OVITO_INSTALL_SHARED_LIB("${dll}")
            IF(${component} MATCHES "Core")
                GET_FILENAME_COMPONENT(QtBinaryPath ${dll} PATH)
                IF(dll MATCHES "Cored.dll$")
                    SET(_qt_dll_suffix "d")
                ELSE()
                    SET(_qt_dll_suffix "")
                ENDIF()
            ENDIF()
        ENDFOREACH()

        # Install Qt plugins required by OVITO.
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platforms/qwindows${_qt_dll_suffix}.dll" DESTINATION "plugins/platforms/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qjpeg${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qgif${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qwebp${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qtiff${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qsvg${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/iconengines/qsvgicon${_qt_dll_suffix}.dll" DESTINATION "plugins/iconengines/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/styles/qmodernwindowsstyle${_qt_dll_suffix}.dll" DESTINATION "plugins/styles/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/qcertonlybackend${_qt_dll_suffix}.dll" DESTINATION "plugins/tls/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/qopensslbackend${_qt_dll_suffix}.dll" DESTINATION "plugins/tls/")
        OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/qschannelbackend${_qt_dll_suffix}.dll" DESTINATION "plugins/tls/")

        # Note: the Direct3D shader compiler DLLs (dxcompiler.dll, dxil.dll) are deliberately NOT
        # deployed. Qt's D3D12 RHI backend would only need them to runtime-compile HLSL shader
        # *source* into DXIL. OVITO instead pre-compiles all of its HLSL shaders to DXIL/DXBC
        # bytecode at build time via the PRECOMPILE option of qt_add_shaders() (see the shader
        # sections of the individual module CMakeLists.txt files). The baked bytecode is consumed
        # directly by the D3D12 backend, so no shader-compiler DLL is loaded at runtime.
    ENDIF()
ENDFUNCTION()
