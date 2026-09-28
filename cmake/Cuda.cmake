# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Enables CUDA support for a CMake target.
MACRO(OVITO_ADD_CUDA_TO_TARGET target_name)
    IF(OVITO_USE_CUDA)
        OVITO_FIND_PACKAGE(CUDAToolkit REQUIRED)

        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC OVITO_USE_CUDA)
        TARGET_LINK_LIBRARIES(${target_name} PUBLIC CUDA::cudart)

        IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CUDA_COMPILER_ID STREQUAL "NVIDIA")
            TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=\"/Zc:__cplusplus\">)
            IF(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "13.0")
                TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=\"/Zc:preprocessor\">)
                TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CPP>:/Zc:preprocessor>)
                TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:C>:/Zc:preprocessor>)
            ENDIF()
        ENDIF()

        TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:
            --use_fast_math
            --relocatable-device-code=true
            --expt-relaxed-constexpr
            $<$<CONFIG:Debug>:--generate-line-info>
        >)
    ENDIF()
ENDMACRO()
