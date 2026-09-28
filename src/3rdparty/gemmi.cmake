# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Build gemmi from source. gemmi is a header-heavy library; we compile a curated subset
# of its .cpp files into a static library to avoid repeated template instantiation costs.
ADD_LIBRARY(gemmi STATIC
            gemmi/src/align.cpp gemmi/src/assembly.cpp gemmi/src/calculate.cpp gemmi/src/ccp4.cpp
            gemmi/src/crd.cpp gemmi/src/ddl.cpp gemmi/src/eig3.cpp gemmi/src/fprime.cpp # gemmi/src/gz.cpp
            gemmi/src/intensit.cpp gemmi/src/json.cpp gemmi/src/mmcif.cpp # gemmi/src/mmread_gz.cpp
            gemmi/src/monlib.cpp gemmi/src/mtz.cpp gemmi/src/mtz2cif.cpp
            gemmi/src/pdb.cpp gemmi/src/polyheur.cpp gemmi/src/read_cif.cpp
            gemmi/src/resinfo.cpp gemmi/src/riding_h.cpp
            gemmi/src/select.cpp gemmi/src/sprintf.cpp gemmi/src/dssp.cpp gemmi/src/symmetry.cpp
            gemmi/src/to_json.cpp gemmi/src/to_mmcif.cpp gemmi/src/to_pdb.cpp gemmi/src/topo.cpp
            gemmi/src/xds_ascii.cpp)
SET_PROPERTY(TARGET gemmi PROPERTY POSITION_INDEPENDENT_CODE ON)
TARGET_COMPILE_DEFINITIONS(gemmi PRIVATE GEMMI_BUILD)
TARGET_INCLUDE_DIRECTORIES(gemmi PUBLIC "${CMAKE_CURRENT_LIST_DIR}/gemmi/include")
TARGET_INCLUDE_DIRECTORIES(gemmi PRIVATE "${CMAKE_CURRENT_LIST_DIR}/gemmi/third_party")
IF(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    TARGET_COMPILE_OPTIONS(gemmi PRIVATE /wd4267) # conversion from 'size_t' to 'unsigned int', possible loss of data
    TARGET_COMPILE_DEFINITIONS(gemmi PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF()
