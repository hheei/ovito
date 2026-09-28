# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# This file defines release version information.

# This is the canonical program version number:
SET(OVITO_VERSION_MAJOR         "3")
SET(OVITO_VERSION_MINOR         "16")
SET(OVITO_VERSION_REVISION      "1")

# The following version number is used to tag the file format of .ovito session state files.
#
# Instructions: Increment the following version counter every time the .ovito file format
# changes in a backward-incompatible way.
#
# Format version 30006 - OVITO ver>3.2.1: TimeAveragingModifier changed.
# Format version 30007 - OVITO ver>3.3.5: New DataObject framework. Removed PropertyStorage class. Introduced PythonScriptDelegate class.
# Format version 30008 - OVITO ver>3.5.4: Added viewport layouts.
# Format version 30009 - OVITO ver>=3.8.0: New DataSet structure (per-viewport Scene, per-scene AnimationSettings, new AnimationTime data type)
# Format version 30010 - OVITO ver>=3.9.0: New property data types (Float32, Float64, Int8)
# Format version 30011 - OVITO ver>=3.9.3: Renamed and/or merged several classes, added RemoteExportSettings to dataset
# Format version 30012 - OVITO ver>=3.11.0: Added reference field flag PROPERTY_FIELD_DONT_SERIALIZE
# Format version 30013 - OVITO ver>=3.12.0: Removed weak reference fields. Serialized classes can be tagged as "nonessential". Split Pipeline class from SceneNode class.
# Format version 30014 - OVITO ver>=3.14.0: Use AnimationFrameLabel class instead of QString to store animation frame labels.
# Format version 30016 - OVITO ver>=3.15.0: Removed support for editable proxy objects. Changed serialization of AnimationFrameLabel structures. Option to serialize only modified object parameters.
# Format version 30017 - OVITO ver>=3.16.0: Changed data type of some scene renderer parameters from int to float.
# Format version 30018 - OVITO ver>=3.16.0: Switched the QDataStream serialization format from QDataStream::Qt_5_4 to QDataStream::Qt_6_10.
# Format version 30019 - OVITO ver>=3.16.1: ColorLegendOverlay displays the 3d shapes of the element types by default.
SET(OVITO_FILE_FORMAT_VERSION "30019")

# The application's default version string:
SET(OVITO_VERSION_STRING "${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}")

# Extract revision number from Git repository to tag development builds of OVITO.
FIND_PACKAGE(Git)
IF(GIT_FOUND AND OVITO_IS_DEVELOPMENT_BUILD)
    # Get the current commit hash:
    EXECUTE_PROCESS(COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short" "HEAD"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE GIT_RESULT_VAR1
        OUTPUT_VARIABLE GIT_COMMIT_REV_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    # Get the current git branch:
    EXECUTE_PROCESS(COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--abbrev-ref" "HEAD"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE GIT_RESULT_VAR2
        OUTPUT_VARIABLE GIT_BRANCH_REV_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    IF(GIT_RESULT_VAR1 STREQUAL "0" AND GIT_RESULT_VAR2 STREQUAL "0")
        SET(OVITO_VERSION_STRING "${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}-dev-${GIT_BRANCH_REV_STRING}-${GIT_COMMIT_REV_STRING}")
    ENDIF()
ENDIF()

# The application's name shown in the main window's title bar:
SET(OVITO_APPLICATION_NAME "Ovito")
IF(OVITO_APPLICATION_NAME_OVERRIDE)
    SET(OVITO_APPLICATION_NAME "${OVITO_APPLICATION_NAME_OVERRIDE}")
ENDIF()

# The copyright notice shown in the application's About dialog:
STRING(TIMESTAMP OVITO_CURRENT_YEAR "%Y")
SET(OVITO_COPYRIGHT_NOTICE
    "<p>A scientific data visualization and analysis software <br>for particle-based simulations.</p>\
     <p>Copyright (C) ${OVITO_CURRENT_YEAR}, OVITO GmbH, Germany</p>\
     <p>\
     This is free, open-source software, and you are welcome to redistribute\
     it under certain conditions. See the user manual for copying conditions.</p>")
IF(OVITO_COPYRIGHT_NOTICE_OVERRIDE)
    SET(OVITO_COPYRIGHT_NOTICE "${OVITO_COPYRIGHT_NOTICE_OVERRIDE}")
ENDIF()

# Export variables to parent scope.
GET_DIRECTORY_PROPERTY(_hasParent PARENT_DIRECTORY)
IF(_hasParent)
    SET(OVITO_VERSION_MAJOR "${OVITO_VERSION_MAJOR}" PARENT_SCOPE)
    SET(OVITO_VERSION_MINOR "${OVITO_VERSION_MINOR}" PARENT_SCOPE)
    SET(OVITO_VERSION_REVISION "${OVITO_VERSION_REVISION}" PARENT_SCOPE)
    SET(OVITO_VERSION_STRING "${OVITO_VERSION_STRING}" PARENT_SCOPE)
    SET(OVITO_APPLICATION_NAME "${OVITO_APPLICATION_NAME}" PARENT_SCOPE)
    SET(OVITO_COPYRIGHT_NOTICE "${OVITO_COPYRIGHT_NOTICE}" PARENT_SCOPE)
ENDIF()