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

# Builds the SQLite3 library from the bundled amalgamation source (sqlite3.c) for static linking.
# OVITO links this private copy into the Particles plugin, where it is used by the MedeA SLI importer.
# Linking SQLite3 statically (instead of against a shared sqlite3 library) avoids a conflict with the
# separate copy of the SQLite3 library that the CPython interpreter loads for its built-in 'sqlite3'
# module when the OVITO Python module is imported into the same process.

ADD_LIBRARY(sqlite3 STATIC sqlite3.c)

# Provide the same imported-target name that CMake's FindSQLite3 module would create,
# so that consumer code can link against SQLite3::SQLite3 regardless of how the library was obtained.
ADD_LIBRARY(SQLite3::SQLite3 ALIAS sqlite3)

# Public include directory for the sqlite3.h header.
TARGET_INCLUDE_DIRECTORIES(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Build as position-independent code so the static archive can be linked into OVITO's shared plugin
# libraries and Python extension modules.
SET_PROPERTY(TARGET sqlite3 PROPERTY POSITION_INDEPENDENT_CODE ON)

# Keep all SQLite3 symbols private to whatever library links this static archive.
# This is essential to prevent the symbols from being re-exported by the Particles plugin / Python
# extension module, where they could clash (via symbol interposition on Linux/macOS) with the
# SQLite3 library used by the host CPython interpreter.
SET_TARGET_PROPERTIES(sqlite3 PROPERTIES C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# SQLite compile-time options. OVITO only reads SQLite database files, so a minimal,
# security-conscious configuration is sufficient.
TARGET_COMPILE_DEFINITIONS(sqlite3 PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION=1    # OVITO never loads SQLite extensions; also avoids a libdl dependency.
    SQLITE_DQS=0                    # Disable the legacy double-quoted-string-literal misfeature.
    SQLITE_DEFAULT_MEMSTATUS=0      # Disable memory statistics tracking (small performance gain).
    SQLITE_THREADSAFE=1             # Serialized threading mode (safe to use from OVITO's worker threads).
)

IF(MSVC)
    # Silence warnings originating from the third-party amalgamation source.
    TARGET_COMPILE_DEFINITIONS(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
    TARGET_COMPILE_OPTIONS(sqlite3 PRIVATE /wd4996 /wd4244 /wd4267)
ENDIF()

# On Unix-like systems SQLite needs pthreads (for SQLITE_THREADSAFE) and libm.
IF(UNIX)
    FIND_PACKAGE(Threads REQUIRED)
    TARGET_LINK_LIBRARIES(sqlite3 PUBLIC Threads::Threads m)
ENDIF()

# Note: The SQLite version is reported in the software bill of materials (SBOM) via the registry entry
# in cmake/dependencies.toml, which extracts it directly from the bundled sqlite3.h header.
