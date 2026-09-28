###############################################################################
#
#  Copyright 2026 OVITO GmbH, Germany
#
#  CMake helper macros for registering OVITO tests with CTest.
#
#  Provides:
#    ovito_add_python_test       — Python integration test via ovitos (+extern)
#    ovito_add_state_render_test — State-file render + image comparison
#    ovito_add_snippet_test      — Documentation snippet smoke test (+extern)
#    ovito_add_codegen_test      — Code-generation round-trip test
#    ovito_add_cpp_test          — C++ QTest unit test
#
###############################################################################

# Look for Qt6::Test to enable C++ unit tests.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Test QUIET)
OPTION(OVITO_BUILD_CPP_TESTS "Build C++ unit tests (requires Qt6::Test)" "${Qt6Test_FOUND}")

# Internal helper: set standard environment variables on a registered Python test.
# Deliberately does NOT set OVITO_PYTHONPATH/PYTHONPATH — callers append it themselves.
function(_ovito_set_python_env test_name)
    # Tell PySide6 which Qt binding to use.
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT "QT_API=pyside6")
    # Disable stream buffering so output appears immediately in CTest logs.
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT "PYTHONUNBUFFERED=1")
    # On macOS debug builds, prefer the debug Qt framework variant.
    if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT "DYLD_IMAGE_SUFFIX=_debug")
    endif()
    # On macOS, allow the embedded interpreter to use the standard user site directory
    # so that architecture-specific NumPy installs are found correctly.
    if(APPLE)
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT "OVITO_USE_STANDARD_PYTHONUSERBASE=1")
    endif()
endfunction()

# Internal helper: Qt plugin search path for "_extern" tests (system Python interpreter).
# Unlike ovitos, the system interpreter has no qt.conf next to its executable, so on Linux
# Qt cannot locate the "ovitoheadless" platform plugin (built into plugins_qt/platforms/)
# unless QT_PLUGIN_PATH points there explicitly.
function(_ovito_set_extern_qt_env test_name)
    if(LINUX)
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "QT_PLUGIN_PATH=${OVITO_LIBRARY_DIRECTORY}/plugins_qt")
    endif()
endfunction()

# Platform-appropriate path separator for PYTHONPATH.
if(WIN32)
    set(_ovito_pathsep ";")
else()
    set(_ovito_pathsep ":")
endif()

# Register a Python integration test.
#
#   ovito_add_python_test(
#       NAME script.py              # Test script filename (relative to WORKING_DIRECTORY)
#       [WORKING_DIRECTORY dir]     # Defaults to ${CMAKE_SOURCE_DIR}/tests/scripts
#       [SLOW]                      # Mark as slow; omits "quick", adds "slow" label
#       [TIMEOUT seconds]           # Override CTest's default TIMEOUT (1500s) for this test
#       [LABELS label1 label2 ...]  # CTest labels (always includes "python")
#   )
#
# Registers two tests:
#   "<script>"         — runs via ovitos; gets "quick" label unless SLOW is set
#   "<script>_extern"  — runs via the system Python interpreter (non-Windows shared
#                        builds only, and only when sanitizers are not active);
#                        never gets "quick" (extern runs are excluded from fast loops)
function(ovito_add_python_test)
    if(NOT OVITO_BUILD_PROFESSIONAL)
        return() # Return early if we are not building OVITO Pro and Python support is unavailable.
    endif()
    cmake_parse_arguments(PARSE_ARGV 0 ARG "SLOW" "NAME;SCRIPT;WORKING_DIRECTORY;TIMEOUT" "LABELS")
    if(NOT ARG_WORKING_DIRECTORY)
        set(ARG_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts")
    endif()
    if(NOT ARG_SCRIPT)
        set(ARG_SCRIPT "${ARG_NAME}")
    endif()
    if(ARG_SLOW)
        set(_speed_label "slow")
    else()
        set(_speed_label "quick")
    endif()

    # The "${ARG_NAME}" and "${ARG_NAME}_extern" variants run the same script in the same
    # WORKING_DIRECTORY and therefore may read/write the same relative-path output files
    # (e.g. "_export_file_test.dump"). Give them a shared RESOURCE_LOCK so CTest never
    # schedules them concurrently, even under `ctest -j`, without limiting parallelism
    # against unrelated scripts in the same directory.
    set(_resource_lock "${ARG_WORKING_DIRECTORY}/${ARG_SCRIPT}")

    add_test(NAME "${ARG_NAME}"
        WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
        COMMAND "$<TARGET_FILE:ovitos>" "${ARG_SCRIPT}")
    _ovito_set_python_env("${ARG_NAME}")
    # ovitos manages its own Python path; OVITO_PYTHONPATH adds the shared test utilities.
    set_property(TEST "${ARG_NAME}" APPEND PROPERTY ENVIRONMENT
        "OVITO_PYTHONPATH=${CMAKE_SOURCE_DIR}/tests/python")
    set_property(TEST "${ARG_NAME}" APPEND PROPERTY LABELS "python" "${_speed_label}" ${ARG_LABELS})
    set_property(TEST "${ARG_NAME}" APPEND PROPERTY RESOURCE_LOCK "${_resource_lock}")
    # A test script may self-skip (e.g. via the @requires_gpu decorator) by printing a "SKIP:"
    # line and exiting cleanly; CTest then reports it as "Not Run" instead of "Failed"/"Passed".
    # SKIP_RETURN_CODE is kept as a secondary signal for runners (e.g. "_extern", below) that
    # don't collapse the script's exit code the way ovitos does. See tests/CLAUDE.md.
    set_property(TEST "${ARG_NAME}" PROPERTY SKIP_RETURN_CODE 125)
    set_property(TEST "${ARG_NAME}" PROPERTY SKIP_REGULAR_EXPRESSION "SKIP:")
    if(ARG_TIMEOUT)
        set_property(TEST "${ARG_NAME}" PROPERTY TIMEOUT "${ARG_TIMEOUT}")
    endif()

    if(BUILD_SHARED_LIBS AND NOT WIN32
            AND NOT OVITO_USE_ADDRESS_SANITIZER AND NOT OVITO_USE_THREAD_SANITIZER)
        add_test(NAME "${ARG_NAME}_extern"
            WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
            COMMAND "${Python3_EXECUTABLE}" "${ARG_SCRIPT}")
        _ovito_set_python_env("${ARG_NAME}_extern")
        _ovito_set_extern_qt_env("${ARG_NAME}_extern")
        # External Python needs the OVITO module directory as well as the shared utilities.
        set_property(TEST "${ARG_NAME}_extern" APPEND PROPERTY ENVIRONMENT
            "PYTHONPATH=${OVITO_PYTHON_DIRECTORY}${_ovito_pathsep}${CMAKE_SOURCE_DIR}/tests/python")
        # _extern tests are never "quick": they add system-Python overhead and are excluded
        # from fast feedback loops via -LE extern.
        set_property(TEST "${ARG_NAME}_extern" APPEND PROPERTY LABELS "python" "extern" ${ARG_LABELS})
        set_property(TEST "${ARG_NAME}_extern" APPEND PROPERTY RESOURCE_LOCK "${_resource_lock}")
        set_property(TEST "${ARG_NAME}_extern" PROPERTY SKIP_RETURN_CODE 125)
        set_property(TEST "${ARG_NAME}_extern" PROPERTY SKIP_REGULAR_EXPRESSION "SKIP:")
        if(ARG_TIMEOUT)
            set_property(TEST "${ARG_NAME}_extern" PROPERTY TIMEOUT "${ARG_TIMEOUT}")
        endif()
    endif()
endfunction()

# Register an OVITO state-file image-comparison test.
#
#   ovito_add_state_render_test(
#       NAME state.ovito            # State file filename (relative to its parent directory)
#       DIRECTORY dir               # Directory containing the state file
#       [BACKEND qrhi]               # Renderer backend (currently only qrhi is supported)
#       [LABELS label1 ...]         # CTest labels (always includes "render")
#   )
function(ovito_add_state_render_test)
    if(NOT OVITO_BUILD_PROFESSIONAL)
        return() # Return early if we are not building OVITO Pro and Python support is unavailable.
    endif()
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;DIRECTORY;BACKEND" "LABELS")
    if(NOT ARG_BACKEND)
        set(ARG_BACKEND "qrhi")
    endif()
    # VisRTX requires an NVIDIA CUDA-capable GPU and isn't even compiled on macOS
    # (see proprietary/ovito/anari/CMakeLists.txt). Don't register these tests there;
    # on Linux/Windows the test script itself detects a missing GPU at runtime and skips.
    if(ARG_BACKEND STREQUAL "visrtx" AND (APPLE OR NOT OVITO_BUILD_PLUGIN_ANARI))
        return()
    endif()
    set(_render_script "${CMAKE_SOURCE_DIR}/tests/files/states/render_and_compare.py")
    set(_test_name "${ARG_BACKEND}_${ARG_NAME}")
    add_test(NAME "${_test_name}"
        WORKING_DIRECTORY "${ARG_DIRECTORY}"
        COMMAND "$<TARGET_FILE:ovitos>" "-o" "${ARG_NAME}" "${_render_script}" "${ARG_NAME}")
    _ovito_set_python_env("${_test_name}")
    set_property(TEST "${_test_name}" APPEND PROPERTY ENVIRONMENT
        "OVITO_PYTHONPATH=${CMAKE_SOURCE_DIR}/tests/python")
    set_property(TEST "${_test_name}" APPEND PROPERTY LABELS "render" ${ARG_LABELS})
    # render_and_compare.py self-skips (e.g. required GPU backend unavailable) by printing a
    # "SKIP:" line and exiting cleanly (ovitos collapses every nonzero exit code to 1, so a
    # numeric SKIP_RETURN_CODE can't be relied on here); CTest then reports the test as
    # "Not Run" instead of "Passed"/"Failed". See tests/CLAUDE.md.
    set_property(TEST "${_test_name}" PROPERTY SKIP_RETURN_CODE 125)
    set_property(TEST "${_test_name}" PROPERTY SKIP_REGULAR_EXPRESSION "SKIP:")
endfunction()

# Register a documentation snippet smoke test.
#
#   ovito_add_snippet_test(
#       NAME relative/snippet.py        # Snippet path relative to the snippets directory
#       [WORKING_DIRECTORY dir]         # Defaults to ${CMAKE_SOURCE_DIR}/docs/python/example_snippets
#       [LABELS label1 ...]             # CTest labels (always includes "snippet")
#   )
#
# Registers:
#   "snippet_<name>"         — runs via ovitos
#   "snippet_<name>_extern"  — runs via the system Python interpreter (same conditions as above)
function(ovito_add_snippet_test)
    if(NOT OVITO_BUILD_PROFESSIONAL)
        return() # Return early if we are not building OVITO Pro and Python support is unavailable.
    endif()
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;WORKING_DIRECTORY" "LABELS")
    if(NOT ARG_WORKING_DIRECTORY)
        set(ARG_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/docs/python/example_snippets")
    endif()
    set(_test_name "snippet_${ARG_NAME}")
    # See ovito_add_python_test() for why the primary and "_extern" variants, which run the
    # same script in the same WORKING_DIRECTORY, share a RESOURCE_LOCK.
    set(_resource_lock "${ARG_WORKING_DIRECTORY}/${ARG_NAME}")

    add_test(NAME "${_test_name}"
        WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
        COMMAND "$<TARGET_FILE:ovitos>" "${ARG_NAME}")
    _ovito_set_python_env("${_test_name}")
    set_property(TEST "${_test_name}" APPEND PROPERTY ENVIRONMENT
        "OVITO_PYTHONPATH=${CMAKE_SOURCE_DIR}/tests/python")
    set_property(TEST "${_test_name}" APPEND PROPERTY LABELS "snippet" ${ARG_LABELS})
    set_property(TEST "${_test_name}" APPEND PROPERTY RESOURCE_LOCK "${_resource_lock}")

    if(BUILD_SHARED_LIBS AND NOT WIN32
            AND NOT OVITO_USE_ADDRESS_SANITIZER AND NOT OVITO_USE_THREAD_SANITIZER)
        add_test(NAME "${_test_name}_extern"
            WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
            COMMAND "${Python3_EXECUTABLE}" "${ARG_NAME}")
        _ovito_set_python_env("${_test_name}_extern")
        _ovito_set_extern_qt_env("${_test_name}_extern")
        set_property(TEST "${_test_name}_extern" APPEND PROPERTY ENVIRONMENT
            "PYTHONPATH=${OVITO_PYTHON_DIRECTORY}${_ovito_pathsep}${CMAKE_SOURCE_DIR}/tests/python")
        set_property(TEST "${_test_name}_extern" APPEND PROPERTY LABELS "snippet" "extern" ${ARG_LABELS})
        set_property(TEST "${_test_name}_extern" APPEND PROPERTY RESOURCE_LOCK "${_resource_lock}")
    endif()
endfunction()

# Register the code-generation round-trip test.
#
#   ovito_add_codegen_test(
#       [LABELS label1 ...]   # CTest labels (always includes "codegen")
#   )
function(ovito_add_codegen_test)
    if(NOT OVITO_BUILD_PROFESSIONAL)
        return() # Return early if we are not building OVITO Pro and Python support is unavailable.
    endif()
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "" "LABELS")

    add_test(NAME "CodeGeneratorTest"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/code_gen"
        COMMAND "$<TARGET_FILE:ovitos>" "${CMAKE_SOURCE_DIR}/tests/code_gen/run_all_tests.py")
    _ovito_set_python_env("CodeGeneratorTest")
    set_property(TEST "CodeGeneratorTest" APPEND PROPERTY ENVIRONMENT
        "OVITO_PYTHONPATH=${CMAKE_SOURCE_DIR}/tests/python")
    set_property(TEST "CodeGeneratorTest" APPEND PROPERTY LABELS "codegen" ${ARG_LABELS})
endfunction()

# Register a C++ QTest unit test executable.
#
#   ovito_add_cpp_test(
#       NAME target_name           # Name of the test executable CMake target
#       SOURCES src1.cpp ...       # Source files
#       LINK_LIBRARIES lib1 ...    # Libraries to link against
#       [LABELS label1 ...]        # CTest labels (always includes "cpp" and "quick")
#   )
function(ovito_add_cpp_test)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME" "SOURCES;LINK_LIBRARIES;LABELS")

    add_executable("${ARG_NAME}" ${ARG_SOURCES})
    target_link_libraries("${ARG_NAME}" PRIVATE Qt6::Test ${ARG_LINK_LIBRARIES})
    OVITO_ADD_STANDARD_COMPILE_OPTIONS("${ARG_NAME}")

    # Set C++23 standard and enable AUTOMOC for the QTEST_MAIN macro.
    set_target_properties("${ARG_NAME}" PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        AUTOMOC ON)

    add_test(NAME "${ARG_NAME}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND "${ARG_NAME}")
    set_property(TEST "${ARG_NAME}" APPEND PROPERTY LABELS "cpp" "quick" ${ARG_LABELS})
endfunction()
