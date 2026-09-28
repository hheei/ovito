# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Enables SYCL support for a CMake target.
MACRO(OVITO_ADD_SYCL_TO_TARGET target_name)

    # Enable SYCL.
    IF(OVITO_USE_SYCL STREQUAL AdaptiveCpp)
        OVITO_FIND_PACKAGE(AdaptiveCpp CONFIG REQUIRED)
        ADD_SYCL_TO_TARGET(TARGET ${target_name})
        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC HIPSYCL_DEBUG_LEVEL=${ADAPTIVECPP_DEBUG_LEVEL})
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "$<$<CONFIG:Debug>:-O0>") # To silcense acpp warning: No optimization flag was given, optimizations are disabled by default.
    ELSEIF(OVITO_USE_SYCL STREQUAL DPC++)
        #ADD_SYCL_TO_TARGET(TARGET ${target_name})
        IF(WIN32)
            TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-fsycl")
            TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-fsycl-targets=nvptx64-nvidia-cuda")
            TARGET_LINK_LIBRARIES(${target_name} PUBLIC "-fsycl")
            TARGET_LINK_LIBRARIES(${target_name} PUBLIC "-fsycl-targets=nvptx64-nvidia-cuda")
        ELSE()
            TARGET_LINK_LIBRARIES(${target_name} PUBLIC IntelSYCL::SYCL_CXX)
            TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-fsycl-targets=nvptx64-nvidia-cuda")
            TARGET_LINK_OPTIONS(${target_name} PUBLIC "-fsycl-targets=nvptx64-nvidia-cuda")
        ENDIF()
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-undefined-var-template")
    ELSEIF(NOT OVITO_USE_SYCL STREQUAL None AND NOT OVITO_USE_SYCL STREQUAL OFF)
        MESSAGE(FATAL_ERROR "Invalid OVITO_USE_SYCL setting. Must be one of [None, AdaptiveCpp, DPC++].")
    ENDIF()

ENDMACRO()
