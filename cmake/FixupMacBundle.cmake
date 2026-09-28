# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Determine the main Qt installation directory, which contains the Qt plugin libraries to be shipped with OVITO.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)
SET(_qt_source_dir "${_qt_import_prefix}")
GET_FILENAME_COMPONENT(_qt_source_dir "${_qt_source_dir}" PATH)
GET_FILENAME_COMPONENT(_qt_source_dir "${_qt_source_dir}" PATH)
GET_FILENAME_COMPONENT(_qt_source_dir "${_qt_source_dir}" PATH)
SET(_qtplugins_source_dir "${_qt_source_dir}/plugins")
SET(QT_LIBRARY_DIRS "${_qt_source_dir}/lib")

# Install needed Qt plugins by copying directories from the Qt installation
SET(_qtplugins_dest_dir "${MACOSX_BUNDLE_NAME}.app/Contents/PlugIns")
INSTALL(DIRECTORY "${_qtplugins_source_dir}/imageformats" DESTINATION ${_qtplugins_dest_dir} PATTERN "*.dSYM" EXCLUDE)
INSTALL(DIRECTORY "${_qtplugins_source_dir}/platforms" DESTINATION ${_qtplugins_dest_dir} PATTERN "*.dSYM" EXCLUDE)
INSTALL(DIRECTORY "${_qtplugins_source_dir}/iconengines" DESTINATION ${_qtplugins_dest_dir} PATTERN "*.dSYM" EXCLUDE)
INSTALL(DIRECTORY "${_qtplugins_source_dir}/styles" DESTINATION ${_qtplugins_dest_dir} PATTERN "*.dSYM" EXCLUDE)
INSTALL(DIRECTORY "${_qtplugins_source_dir}/networkinformation" DESTINATION ${_qtplugins_dest_dir} PATTERN "*.dSYM" EXCLUDE)
INSTALL(DIRECTORY "${_qtplugins_source_dir}/tls" DESTINATION ${_qtplugins_dest_dir} PATTERN "*.dSYM" EXCLUDE)

# Install the Qt framework modules that OVITO does not link against itself and which FIXUP_BUNDLE() can
# therefore not discover by walking the binary dependencies of the program: The bundled PySide6 package
# provides Python bindings for these modules, but its extension modules are on the IGNORE_ITEM list of
# FIXUP_BUNDLE() below, which suppresses the scan of their prerequisites.
# The frameworks are copied in the same reduced layout that BundleUtilities produces for the other Qt
# frameworks, i.e. without the C++ headers. The code signature is left out, because the install names of
# the framework binaries are rewritten by FIXUP_BUNDLE() below and the bundle is re-signed afterwards.
SET(OVITO_EXTRA_QT_FRAMEWORK_BINARIES "")
IF(OVITO_BUILD_PROFESSIONAL)
    FOREACH(component IN LISTS OVITO_PYSIDE_ONLY_QT_COMPONENTS)
        IF(NOT EXISTS "${QT_LIBRARY_DIRS}/Qt${component}.framework")
            MESSAGE(FATAL_ERROR "Qt framework module required by the bundled PySide6 package not found: ${QT_LIBRARY_DIRS}/Qt${component}.framework")
        ENDIF()
        INSTALL(DIRECTORY "${QT_LIBRARY_DIRS}/Qt${component}.framework"
            DESTINATION "${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks"
            USE_SOURCE_PERMISSIONS
            PATTERN "Headers" EXCLUDE
            PATTERN "*.prl" EXCLUDE
            PATTERN "*.dSYM" EXCLUDE
            REGEX "/_CodeSignature/" EXCLUDE)
        # Remember the location of the installed framework binary, so that it can be handed to FIXUP_BUNDLE() below.
        STRING(APPEND OVITO_EXTRA_QT_FRAMEWORK_BINARIES "\n        \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/Qt${component}.framework/Versions/A/Qt${component}\"")
    ENDFOREACH()
ENDIF()

# Install a qt.conf file.
# This inserts some cmake code into the install script to write the file
INSTALL(CODE "
    file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Resources/qt.conf\" \"[Paths]\\nPlugins = PlugIns/\")
    ")

# Purge any previous version of the nested bundle to avoid errors during bundle fixup.
IF(OVITO_BUILD_PLUGIN_PYSCRIPT)
    INSTALL(CODE "
        FILE(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/MacOS/Ovito.app\")
        ")
ENDIF()

# Use BundleUtilities to get all other dependencies for the application to work.
# It takes a bundle or executable along with possible plugins and inspects it
# for dependencies.  If they are not system dependencies, they are copied.

# Now the work of copying dependencies into the bundle/package
# The quotes are escaped and variables to use at install time have their $ escaped
# An alternative is the do a configure_file() on a script and use install(SCRIPT  ...).
# Note that the image plugins depend on QtSvg and QtXml, and it got those copied
# over.
INSTALL(CODE "
    CMAKE_POLICY(SET CMP0011 NEW)
    CMAKE_POLICY(SET CMP0009 NEW)

    # Use BundleUtilities to get all other dependencies for the application to work.
    # It takes a bundle or executable along with possible plugins and inspects it
    # for dependencies.  If they are not system dependencies, they are copied.
    SET(APPS \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app\")

    # Directories to look for dependencies:
    SET(DIRS
        ${QT_LIBRARY_DIRS}
        \"\${CMAKE_INSTALL_PREFIX}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}\"
        \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks\"
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/imageformats\"
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/platforms\"
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/iconengines\"
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/networkinformation\"
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/styles\"
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/tls\")
    # Note: /opt/local/lib (MacPorts) used to be on this list. Nothing OVITO links comes
    # from a third-party package manager any more -- every macOS dependency is built into
    # the tree that ci/provision/macos/provision.py maintains -- and leaving the prefix here
    # only gave BundleUtilities a way to resolve a stray dependency out of MacPorts and copy
    # it into the shipped bundle.

    # Returns the path that others should refer to the item by when the item is embedded inside a bundle.
    # This ensures that all plugin libraries go into the PlugIns/ directory of the bundle.
    FUNCTION(gp_item_default_embedded_path_override item default_embedded_path_var)
        # Embed plugin libraries (.so) in the PlugIns/ subdirectory:
        IF(item MATCHES \"\\\\${OVITO_PLUGIN_LIBRARY_SUFFIX}$\" AND (item MATCHES \"^@rpath\" OR item MATCHES \"PlugIns/\"))
            SET(path \"@executable_path/../PlugIns\")
            SET(\${default_embedded_path_var} \"\${path}\" PARENT_SCOPE)
            MESSAGE(\"     Embedding path override: \${item} -> \${path}\")
        ENDIF()
        # Leave helper libraries in the MacOS/ directory:
        IF(item MATCHES \"libovito\"  AND item MATCHES \"^@rpath\")
            SET(path \"@executable_path\")
            SET(\${default_embedded_path_var} \"\${path}\" PARENT_SCOPE)
            MESSAGE(\"     Embedding path override: \${item} -> \${path}\")
        ENDIF()
    ENDFUNCTION()

    FUNCTION(gp_resolved_file_type_override resolved_file type)

        # This is needed to correctly install Matplotlib's shared libraries in the .dylibs/ subdirectory:
        IF(resolved_file MATCHES \"@loader_path/\" AND resolved_file MATCHES \"/.dylibs/\")
            SET(\${type} \"system\" PARENT_SCOPE)
        ENDIF()

    ENDFUNCTION()

    FILE(GLOB_RECURSE QTPLUGINS
        \"\${CMAKE_INSTALL_PREFIX}/${_qtplugins_dest_dir}/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
    FILE(GLOB_RECURSE OVITO_PLUGINS
        \"\${CMAKE_INSTALL_PREFIX}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}/*${OVITO_PLUGIN_LIBRARY_SUFFIX}\")
    FILE(GLOB_RECURSE PYTHON_DYNLIBS
        \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/Python.framework/*.so\")
    FILE(GLOB OTHER_DYNLIBS
        \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/MacOS/*.dylib\")
    FILE(GLOB OTHER_FRAMEWORK_DYNLIBS
        \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/*.dylib\")
    FILE(GLOB OSSL_MODULES_DYNLIBS
        \"\${CMAKE_INSTALL_PREFIX}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}/ossl-modules/*.dylib\")
    # The Qt frameworks that were installed above are already in place and must not be copied again by FIXUP_BUNDLE().
    # But listing them as bundle libraries makes FIXUP_BUNDLE() rewrite their install names to @executable_path/../Frameworks/,
    # and, more importantly, lets it redirect the @rpath references to them found in the PySide6 extension modules.
    SET(EXTRA_QT_FRAMEWORK_DYNLIBS ${OVITO_EXTRA_QT_FRAMEWORK_BINARIES})

    FOREACH(lib \${QTPLUGINS} \${OVITO_PLUGINS} \${PYTHON_DYNLIBS} \${OTHER_DYNLIBS} \${OTHER_FRAMEWORK_DYNLIBS} \${OSSL_MODULES_DYNLIBS} \${EXTRA_QT_FRAMEWORK_DYNLIBS})
        IF(NOT IS_SYMLINK \${lib})
            LIST(APPEND BUNDLE_LIBS \${lib})
        ENDIF()
    ENDFOREACH()

    # Include the Python interpreter executable in the IGNORE_ITEM list.
    SET(IGNORE_ITEM_LIST \"Python\")

    # Collect the filenames of the Shiboken and PySide libraries.
    # These shared objects need to be preserved by adding them to the IGNORE_ITEM list of FIXUP_BUNDLE().
    FILE(GLOB PYSIDE_DYNLIBS \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/Python.framework/Versions/*.*/lib/python*/site-packages/PySide*/*.dylib\")
    FILE(GLOB PYSIDE_SOLIBS \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/Python.framework/Versions/*.*/lib/python*/site-packages/PySide*/*.so\")
    FILE(GLOB SHIBOKEN_DYNLIBS \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/Python.framework/Versions/*.*/lib/python*/site-packages/shiboken*/*.dylib\")
    FILE(GLOB SHIBOKEN_SOLIBS \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/Python.framework/Versions/*.*/lib/python*/site-packages/shiboken*/*.so\")
    MESSAGE(\"PYSIDE_DYNLIBS: \${PYSIDE_DYNLIBS}\")
    MESSAGE(\"PYSIDE_SOLIBS: \${PYSIDE_SOLIBS}\")
    MESSAGE(\"SHIBOKEN_DYNLIBS: \${SHIBOKEN_DYNLIBS}\")
    MESSAGE(\"SHIBOKEN_SOLIBS: \${SHIBOKEN_SOLIBS}\")
    FOREACH(lib \${PYSIDE_DYNLIBS} \${PYSIDE_SOLIBS} \${SHIBOKEN_DYNLIBS} \${SHIBOKEN_SOLIBS})
        GET_FILENAME_COMPONENT(lib_filename \"\${lib}\" NAME)
        LIST(APPEND IGNORE_ITEM_LIST \"\${lib_filename}\")
    ENDFOREACH()

    MESSAGE(\"---- Bundle libs for fixup_bundle: \${BUNDLE_LIBS}\")
    SET(BU_CHMOD_BUNDLE_ITEMS ON)   # Make copies of system libraries writable before install_name_tool tries to change them.
    INCLUDE(BundleUtilities)
    FIXUP_BUNDLE(\"\${APPS}\" \"\${BUNDLE_LIBS}\" \"\${DIRS}\" IGNORE_ITEM \${IGNORE_ITEM_LIST})

    # Extend rpath information of the PySide/Shiboken libraries, such that they will be found an runtime.
    SET(QT_LIB_INSTALL_PATH \"${_qt_source_dir}/lib\")
    FOREACH(lib \${PYSIDE_DYNLIBS} \${PYSIDE_SOLIBS} \${SHIBOKEN_DYNLIBS} \${SHIBOKEN_SOLIBS})
        MESSAGE(\"-- Extending rpaths of \${lib}\")
        # Before we can do a -add_rpath, we need to remove any existing rpath entries. Otherwise, install_name_tool may fail with an error due to duplicate rpaths.
        EXECUTE_PROCESS(COMMAND install_name_tool -delete_rpath \"@loader_path/\" \"\${lib}\" COMMAND_ECHO STDERR)
        EXECUTE_PROCESS(COMMAND install_name_tool -delete_rpath \"@loader_path/../shiboken6/\" \"\${lib}\" COMMAND_ECHO STDERR)
        EXECUTE_PROCESS(COMMAND install_name_tool -delete_rpath \"\${QT_LIB_INSTALL_PATH}\" \"\${lib}\" COMMAND_ECHO STDERR)
        EXECUTE_PROCESS(COMMAND install_name_tool -add_rpath \"@executable_path/../Frameworks/\" -add_rpath \"@loader_path/\" -add_rpath \"@loader_path/../shiboken6/\" \"\${lib}\" COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR)
    ENDFOREACH()

")

IF(OVITO_BUILD_PLUGIN_OSPRAY)
    # Extend the rpath information of the rkcommon library such that extension modules loaded via dlopen()
    # are found in the Frameworks/ directory at runtime.
    # The file must be looked up with a glob rather than by a fixed name: FIXUP_BUNDLE() names its copy
    # after the reference recorded in the libraries that link it, so the library arrives here under
    # whatever install name the rkcommon build gave it -- librkcommon.1.14.0.dylib for a stock build, and
    # librkcommon.dylib only if that install name was shortened by hand afterwards.
    INSTALL(CODE "
        FILE(GLOB _rkcommon_libraries \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/librkcommon*.dylib\")
        IF(NOT _rkcommon_libraries)
            MESSAGE(FATAL_ERROR \"No rkcommon library in \${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks/. FIXUP_BUNDLE() should have copied it there as a dependency of the OSPRay libraries.\")
        ENDIF()
        FOREACH(lib IN LISTS _rkcommon_libraries)
            IF(NOT IS_SYMLINK \"\${lib}\")
                MESSAGE(\"-- Adding rpath to \${lib}\")
                EXECUTE_PROCESS(COMMAND install_name_tool -add_rpath \"@loader_path/\" \"\${lib}\" COMMAND_ERROR_IS_FATAL ANY)
            ENDIF()
        ENDFOREACH()
    ")
ENDIF()

IF(OVITO_BUILD_PLUGIN_PYSCRIPT AND NOT OVITO_BUILD_BASIC)

    # Create a nested bundle for 'ovitos'.
    # This is to prevent the program icon from showing up in the dock when 'ovitos' is run.
    INSTALL(CODE "
        SET(BundlePath \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app\")
        EXECUTE_PROCESS(COMMAND \"\${CMAKE_COMMAND}\" -E make_directory \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/MacOS\")
        FILE(RENAME \"\${BundlePath}/Contents/MacOS/ovitos.exe\" \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/MacOS/ovitos\")
        EXECUTE_PROCESS(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink \"../../../Resources\" \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/Resources\")
        EXECUTE_PROCESS(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink \"../../../Frameworks\" \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/Frameworks\")
        EXECUTE_PROCESS(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink \"../../../PlugIns\" \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/PlugIns\")
        CONFIGURE_FILE(\"${Ovito_SOURCE_DIR}/src/main/resources/Info.plist\" \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/Info.plist\")
        EXECUTE_PROCESS(COMMAND defaults write \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/Info\" LSUIElement 1)
        FILE(GLOB DylibsToSymlink \"\${BundlePath}/Contents/MacOS/*.dylib\")
        FOREACH(FILE_ENTRY \${DylibsToSymlink})
            GET_FILENAME_COMPONENT(FILE_ENTRY_NAME \"\${FILE_ENTRY}\" NAME)
            EXECUTE_PROCESS(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink \"../../../\${FILE_ENTRY_NAME}\" \"\${BundlePath}/Contents/MacOS/Ovito.app/Contents/MacOS/\${FILE_ENTRY_NAME}\" COMMAND_ERROR_IS_FATAL ANY)
        ENDFOREACH()
    ")

    # Uninstall PyPI packages from the embedded interpreter that should not be part of the official distribution.
    INSTALL(CODE "${OVITO_UNINSTALL_UNUSED_PYTHON_MODULES_CODE}")
ENDIF()
