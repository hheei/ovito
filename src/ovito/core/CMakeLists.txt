# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find ffmpeg libraries for video encoding support.
IF(NOT EMSCRIPTEN)
    OVITO_FIND_PACKAGE(FFMPEG)
ENDIF()

# Find the zlib, needed for reading/writing compressed data files.
IF(NOT EMSCRIPTEN)
    OVITO_FIND_PACKAGE(ZLIB)
ENDIF()

# Locate the libssh library, needed for the built-in SSH client.
IF(OVITO_BUILD_SSH_CLIENT)
    OVITO_FIND_PACKAGE(libssh REQUIRED)
ENDIF()

# Locate the Boost C++ library.
OVITO_FIND_PACKAGE(Boost REQUIRED)

SET(SourceFiles
    utilities/Exception.cpp
    utilities/linalg/AffineDecomposition.cpp
    utilities/io/SaveStream.cpp
    utilities/io/LoadStream.cpp
    utilities/io/video/VideoEncoder.cpp
    utilities/io/video/ExternalVideoEncoderBackend.cpp
    app/Application.cpp
    app/ApplicationService.cpp
    app/PluginManager.cpp
    app/StandaloneApplication.cpp
    app/UserInterface.cpp
    app/undo/UndoableOperation.cpp
    app/undo/UndoStack.cpp
    oo/CloneHelper.cpp
    oo/OvitoClass.cpp
    oo/OvitoObject.cpp
    oo/PropertyFieldDescriptor.cpp
    oo/PropertyField.cpp
    oo/RefMaker.cpp
    oo/RefMakerClass.cpp
    oo/RefTarget.cpp
    oo/RefTargetListener.cpp
    dataset/animation/AnimationSettings.cpp
    dataset/animation/AnimationFrameLabel.cpp
    dataset/animation/controller/Controller.cpp
    dataset/animation/controller/ConstantControllers.cpp
    dataset/animation/controller/KeyframeController.cpp
    dataset/animation/controller/LinearInterpolationControllers.cpp
    dataset/animation/controller/SplineInterpolationControllers.cpp
    dataset/animation/controller/TCBInterpolationControllers.cpp
    dataset/animation/controller/AnimationKeys.cpp
    dataset/animation/controller/LookAtController.cpp
    dataset/animation/controller/PRSTransformationController.cpp
    dataset/DataSet.cpp
    dataset/DataSetContainer.cpp
    dataset/io/FileImporter.cpp
    dataset/io/FileExporter.cpp
    dataset/io/FileSourceImporter.cpp
    dataset/io/FileSource.cpp
    dataset/io/AttributeFileExporter.cpp
    dataset/scene/SceneNode.cpp
    dataset/scene/Pipeline.cpp
    dataset/scene/Scene.cpp
    dataset/scene/ScenePreparation.cpp
    dataset/scene/SceneAnimationPlayback.cpp
    dataset/scene/SelectionSet.cpp
    dataset/data/DataVis.cpp
    dataset/data/DataObject.cpp
    dataset/data/DataBuffer.cpp
    dataset/data/DataCollection.cpp
    dataset/data/camera/AbstractCameraObject.cpp
    dataset/data/camera/AbstractCameraSource.cpp
    dataset/data/AttributeDataObject.cpp
    dataset/data/mesh/TriangleMesh.cpp
    dataset/data/mesh/TriangleMeshVis.cpp
    dataset/pipeline/ActiveObject.cpp
    dataset/pipeline/PipelineFlowState.cpp
    dataset/pipeline/PipelineCache.cpp
    dataset/pipeline/PipelineNode.cpp
    dataset/pipeline/PipelineStatus.cpp
    dataset/pipeline/Modifier.cpp
    dataset/pipeline/ModifierDelegate.cpp
    dataset/pipeline/ModificationNode.cpp
    dataset/pipeline/ModifierGroup.cpp
    dataset/pipeline/StaticSource.cpp
    dataset/pipeline/BasePipelineSource.cpp
    dataset/pipeline/DelegatingModifier.cpp
    utilities/units/UnitsManager.cpp
    utilities/units/PrescribedScaleUnit.cpp
    utilities/io/ObjectSaveStream.cpp
    utilities/io/ObjectLoadStream.cpp
    utilities/io/FileManager.cpp
    utilities/io/RemoteFileJob.cpp
    utilities/io/CompressedTextReader.cpp
    utilities/io/CompressedTextWriter.cpp
    utilities/concurrent/Task.cpp
    utilities/concurrent/TaskManager.cpp
    utilities/concurrent/TaskProgress.cpp
    utilities/concurrent/MainThreadOperation.cpp
    utilities/concurrent/TaskScope.cpp
    utilities/concurrent/FutureWatcher.cpp
    utilities/io/ssh/SshConnection.cpp
    utilities/io/ssh/openssh/OpensshConnection.cpp
    utilities/io/ssh/openssh/SshRequest.cpp
    utilities/io/ssh/openssh/DownloadRequest.cpp
    utilities/io/ssh/openssh/FileListingRequest.cpp
    utilities/io/video/VideoEncoder.cpp
    utilities/io/video/ExternalVideoEncoderBackend.cpp
    rendering/SceneRenderer.cpp
    rendering/FrameGraph.cpp
    rendering/FrameGraphBuilder.cpp
    rendering/MeshPrimitive.cpp
    rendering/TextPrimitive.cpp
    rendering/RenderSettings.cpp
    rendering/FrameBuffer.cpp
    rendering/ColorCodingGradient.cpp
    rendering/ObjectPickingMap.cpp
    rendering/OpacityFunction.cpp
    rendering/RenderThread.cpp
    rendering/WarningIndicatorRenderer.cpp
)
if(OVITO_BUILD_BASIC)
    list(APPEND SourceFiles rendering/WatermarkRenderer.cpp)
endif()
list(APPEND SourceFiles
    rendering/standard/StandardRenderer.cpp
    rendering/standard/StandardRendererImplementation.cpp
    rendering/standard/OutlinesRhi.cpp
    rendering/standard/PrimitiveRenderer.cpp
    rendering/standard/ImagePrimitiveRenderer.cpp
    rendering/standard/LinePrimitiveRenderer.cpp
    rendering/standard/MarkerPrimitiveRenderer.cpp
    rendering/standard/TextBillboardPrimitiveRenderer.cpp
    rendering/standard/MeshPrimitiveRenderer.cpp
    rendering/standard/ParticlePrimitiveRenderer.cpp
    rendering/standard/CylinderPrimitiveRenderer.cpp
    viewport/Viewport.cpp
    viewport/ViewportWindow.cpp
    viewport/ViewportConfiguration.cpp
    viewport/ViewportLayout.cpp
    viewport/ViewportSettings.cpp
    viewport/overlays/ViewportOverlay.cpp
    viewport/overlays/CoordinateTripodOverlay.cpp
    viewport/overlays/TextLabelOverlay.cpp
)

# Build the askpass utility, which is needed for the external OpenSSH-based client.
IF(OVITO_BUILD_APP AND NOT EMSCRIPTEN)
    ADD_SUBDIRECTORY(utilities/io/ssh/openssh/askpass)
ENDIF()

# Make use of the zlib library provided by Emscripten.
IF(EMSCRIPTEN)
    MESSAGE("Using Emscripten port of zlib library.")
    ADD_LIBRARY(ZLIB::ZLIB INTERFACE IMPORTED)
    TARGET_COMPILE_OPTIONS(ZLIB::ZLIB INTERFACE "SHELL:-s USE_ZLIB=1")
    SET(ZLIB_FOUND TRUE)
ENDIF()

# Define library target.
OVITO_STANDARD_PLUGIN(Core
    SOURCES ${SourceFiles} resources/core.qrc
    LIB_DEPENDENCIES Boost::boost function2::function2
    PRECOMPILED_HEADERS Core.h
)

# Compile GLSL shaders for the QRhi-based renderer.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS ShaderTools REQUIRED)

# All qt_add_shaders() calls below pass the PRECOMPILE option. On Windows this makes qsb
# pre-compile the generated HLSL to DXIL bytecode at build time (via dxc, since all of OVITO's
# shaders target HLSL Shader Model 6.0) and embed it in the .qsb files. The D3D12 RHI backend
# then consumes the ready-made bytecode directly, so the deployed application does NOT need to
# runtime-compile HLSL and therefore does NOT depend on dxcompiler.dll/dxil.dll being shipped
# alongside it. (On Linux/macOS PRECOMPILE is a no-op: qsb always embeds SPIR-V, which Vulkan
# consumes directly, and MSL is compiled at runtime by the OS Metal framework, which needs no
# redistributable.)
#
# Caveat: qsb locates dxc via the PATH environment variable and does NOT fail the build when it
# is missing -- it silently leaves the HLSL *source* in the .qsb, which would re-introduce the
# runtime dependency on the shader-compiler DLLs. Guard against that by requiring dxc at configure
# time. A known-good dxc.exe ships with the Vulkan SDK (in its Bin directory) and with recent
# Windows SDKs.
IF(WIN32)
    FIND_PROGRAM(OVITO_DXC_EXECUTABLE NAMES dxc)
    IF(NOT OVITO_DXC_EXECUTABLE)
        MESSAGE(FATAL_ERROR
            "The DirectX shader compiler 'dxc' was not found on the PATH. It is required at build "
            "time to pre-compile OVITO's Shader-Model 6.0 HLSL shaders to DXIL, so that the deployed "
            "application does not depend on dxcompiler.dll/dxil.dll at runtime. Install the Vulkan SDK "
            "or a recent Windows SDK and make sure its dxc.exe is on the PATH, then re-run CMake.")
    ENDIF()
ENDIF()

# Workaround for a SPIRV-Cross bug: when a GLSL fragment shader declares
# layout(depth_greater), SPIRV-Cross emits SV_DepthGreaterEqual in the HLSL output
# but does NOT add the linear_noperspective_centroid qualifier to the SV_Position
# input, which DXIL validation on Direct3D requires in this combination.  The
# result is a runtime HLSL compilation failure on Direct3D.
#
# We sacrifice the HiZ/early-Z optimization on Windows by compiling all raycast
# shaders with OVITO_DISABLE_DEPTH_GREATER, which keeps plain gl_FragDepth
# (→ SV_Depth in HLSL) and avoids the validation error.  The optimization itself
# would be equally useful on Direct3D, so this is a pure workaround, not a
# principled decision.
#
# Root cause: spirv_hlsl.cpp / CompilerHLSL::emit_builtin_inputs_in_struct(),
# BuiltInFragCoord case — needs to prepend "linear noperspective centroid" to the
# float4 type when ExecutionModeDepthGreater or ExecutionModeDepthLess is active.
# As of Qt 6.11.1 (June 2026) this fix is not yet present in Qt's bundled
# SPIRV-Cross.  Revisit when a Qt release ships an updated SPIRV-Cross that
# includes the fix; at that point this block and the #ifndef guards in the seven
# affected surface snippets can be removed.
if(WIN32)
    set(_OVITO_DEPTH_GREATER_SHADER_DEFINES DEFINES OVITO_DISABLE_DEPTH_GREATER)
endif()

# Helpers for the modular particle shader system.
# Vertex and fragment stubs are generated into shaders/particles/variants/ (gitignored source tree)
# from template files. Paths accumulate in the global property OVITO_CORE_PARTICLE_VARIANT_SHADERS.

set(_OVITO_PARTICLE_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rendering/standard/shaders")
set(_OVITO_PARTICLE_VARIANT_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/standard/shaders/particles/variants")

# Generate a vertex shader stub for a (attribs, geometry) pair.
# Usage: ovito_particle_vert(NAME ATTRIBS <file> GEOMETRY <file>)
function(ovito_particle_vert VERT_NAME)
    cmake_parse_arguments(ARG "" "ATTRIBS;GEOMETRY" "" ${ARGN})
    set(ATTRIBS_SNIPPET  "${ARG_ATTRIBS}")
    set(GEOMETRY_SNIPPET "${ARG_GEOMETRY}")
    file(MAKE_DIRECTORY "${_OVITO_PARTICLE_VARIANT_DIR}")
    set(VERT_OUT "${_OVITO_PARTICLE_VARIANT_DIR}/${VERT_NAME}.vert")
    configure_file("${_OVITO_PARTICLE_SHADER_SRC_DIR}/particle_stub.vert.in" "${VERT_OUT}" @ONLY)
    set_property(GLOBAL APPEND PROPERTY OVITO_CORE_PARTICLE_VARIANT_SHADERS "${VERT_OUT}")
endfunction()

# Generate a fragment shader stub for a (surface, fragout) pair.
# Usage: ovito_particle_frag(NAME SURFACE <file> FRAGOUT <file>)
function(ovito_particle_frag FRAG_NAME)
    cmake_parse_arguments(ARG "" "SURFACE;FRAGOUT" "" ${ARGN})
    set(SURFACE_SNIPPET "${ARG_SURFACE}")
    set(FRAGOUT_SNIPPET "${ARG_FRAGOUT}")
    file(MAKE_DIRECTORY "${_OVITO_PARTICLE_VARIANT_DIR}")
    set(FRAG_OUT "${_OVITO_PARTICLE_VARIANT_DIR}/${FRAG_NAME}.frag")
    configure_file("${_OVITO_PARTICLE_SHADER_SRC_DIR}/particle_stub.frag.in" "${FRAG_OUT}" @ONLY)
    set_property(GLOBAL APPEND PROPERTY OVITO_CORE_PARTICLE_VARIANT_SHADERS "${FRAG_OUT}")
endfunction()

# Canonical particle shader variant table. Calls ovito_particle_vert() / ovito_particle_frag()
# to register every (geometry, attribs, surface, fragout) combination.
include("${CMAKE_CURRENT_SOURCE_DIR}/rendering/standard/shaders/particle_variants.cmake")

# Compile any registered variant stubs.
# BASE strips the variant dir prefix so each shader lands at:
#   :/ovito/core/rendering/standard/shaders/particles/variants/<name>.<stage>.qsb
get_property(_PARTICLE_SHADER_FILES GLOBAL PROPERTY OVITO_CORE_PARTICLE_VARIANT_SHADERS)
if(_PARTICLE_SHADER_FILES)
    qt_add_shaders(Core "core_rhi_particle_variants"
        PRECOMPILE
        GLSL "450"
        HLSL "60"
        MSL "12"
        BASE "${_OVITO_PARTICLE_VARIANT_DIR}"
        PREFIX "/ovito/core/rendering/standard/shaders/particles/variants"
        ${_OVITO_DEPTH_GREATER_SHADER_DEFINES}
        FILES
            ${_PARTICLE_SHADER_FILES}
    )
endif()

# Helpers for the modular cylinder/arrow shader system.
# Vertex and fragment stubs are generated into shaders/cylinders/variants/ (gitignored source tree)
# from template files. Paths accumulate in the global property OVITO_CORE_CYLINDER_VARIANT_SHADERS.

set(_OVITO_CYLINDER_VARIANT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rendering/standard/shaders/cylinders/variants")

# Generate a vertex shader stub for a (attribs, geometry) pair.
# Usage: ovito_cylinder_vert(NAME ATTRIBS <file> GEOMETRY <file>)
function(ovito_cylinder_vert VERT_NAME)
    cmake_parse_arguments(ARG "" "ATTRIBS;GEOMETRY" "" ${ARGN})
    set(ATTRIBS_SNIPPET  "${ARG_ATTRIBS}")
    set(GEOMETRY_SNIPPET "${ARG_GEOMETRY}")
    file(MAKE_DIRECTORY "${_OVITO_CYLINDER_VARIANT_DIR}")
    set(VERT_OUT "${_OVITO_CYLINDER_VARIANT_DIR}/${VERT_NAME}.vert")
    configure_file("${_OVITO_PARTICLE_SHADER_SRC_DIR}/cylinder_stub.vert.in" "${VERT_OUT}" @ONLY)
    set_property(GLOBAL APPEND PROPERTY OVITO_CORE_CYLINDER_VARIANT_SHADERS "${VERT_OUT}")
endfunction()

# Generate a fragment shader stub for a (surface, fragout) pair.
# Usage: ovito_cylinder_frag(NAME SURFACE <file> FRAGOUT <file>)
function(ovito_cylinder_frag FRAG_NAME)
    cmake_parse_arguments(ARG "" "SURFACE;FRAGOUT" "" ${ARGN})
    set(SURFACE_SNIPPET "${ARG_SURFACE}")
    set(FRAGOUT_SNIPPET "${ARG_FRAGOUT}")
    file(MAKE_DIRECTORY "${_OVITO_CYLINDER_VARIANT_DIR}")
    set(FRAG_OUT "${_OVITO_CYLINDER_VARIANT_DIR}/${FRAG_NAME}.frag")
    configure_file("${_OVITO_PARTICLE_SHADER_SRC_DIR}/cylinder_stub.frag.in" "${FRAG_OUT}" @ONLY)
    set_property(GLOBAL APPEND PROPERTY OVITO_CORE_CYLINDER_VARIANT_SHADERS "${FRAG_OUT}")
endfunction()

# Generate a pseudo-color fragment shader stub: adds colorMap sampler (binding 2) and applies
# the transfer function per-fragment after surface interpolation.
# Usage: ovito_cylinder_frag_pseudo(NAME SURFACE <file> FRAGOUT <file>)
function(ovito_cylinder_frag_pseudo FRAG_NAME)
    cmake_parse_arguments(ARG "" "SURFACE;FRAGOUT" "" ${ARGN})
    set(SURFACE_SNIPPET "${ARG_SURFACE}")
    set(FRAGOUT_SNIPPET "${ARG_FRAGOUT}")
    file(MAKE_DIRECTORY "${_OVITO_CYLINDER_VARIANT_DIR}")
    set(FRAG_OUT "${_OVITO_CYLINDER_VARIANT_DIR}/${FRAG_NAME}.frag")
    configure_file("${_OVITO_PARTICLE_SHADER_SRC_DIR}/cylinder_stub_pseudo.frag.in" "${FRAG_OUT}" @ONLY)
    set_property(GLOBAL APPEND PROPERTY OVITO_CORE_CYLINDER_VARIANT_SHADERS "${FRAG_OUT}")
endfunction()

# Canonical cylinder shader variant table.
include("${CMAKE_CURRENT_SOURCE_DIR}/rendering/standard/shaders/cylinder_variants.cmake")

# Compile any registered variant stubs.
get_property(_CYLINDER_SHADER_FILES GLOBAL PROPERTY OVITO_CORE_CYLINDER_VARIANT_SHADERS)
if(_CYLINDER_SHADER_FILES)
    qt_add_shaders(Core "core_rhi_cylinder_variants"
        PRECOMPILE
        GLSL "450"
        HLSL "60"
        MSL "12"
        BASE "${_OVITO_CYLINDER_VARIANT_DIR}"
        PREFIX "/ovito/core/rendering/standard/shaders/cylinders/variants"
        ${_OVITO_DEPTH_GREATER_SHADER_DEFINES}
        FILES
            ${_CYLINDER_SHADER_FILES}
    )
endif()

qt_add_shaders(Core "core_rhi_renderer_shaders"
    PRECOMPILE
    GLSL "450"
    HLSL "60"
    MSL "12"
    PREFIX "/ovito/core"
    FILES
        rendering/standard/shaders/warning_indicator.vert
        rendering/standard/shaders/warning_indicator.frag
        rendering/standard/shaders/image_quad.vert
        rendering/standard/shaders/image_quad.frag
        rendering/standard/shaders/postprocess_passthrough.vert
        rendering/standard/shaders/postprocess_passthrough.frag
        rendering/standard/shaders/postprocess_outline.vert
        rendering/standard/shaders/postprocess_outline.frag
        rendering/standard/shaders/outlines_init.comp
        rendering/standard/shaders/outlines_cleanup.comp
        rendering/standard/shaders/outlines_setup.comp
        rendering/standard/shaders/outlines_expand.comp
        rendering/standard/shaders/outlines_resolve.comp
        rendering/standard/shaders/outlines_jfa_seed.comp
        rendering/standard/shaders/outlines_jfa_flood.comp
        rendering/standard/shaders/outlines_jfa_resolve.comp
        rendering/standard/shaders/oit_compose.vert
        rendering/standard/shaders/oit_compose.frag
        rendering/standard/shaders/line_thin.vert
        rendering/standard/shaders/line_thin.frag
        rendering/standard/shaders/line_depth.frag
        rendering/standard/shaders/line_thin_picking.vert
        rendering/standard/shaders/line_thin_picking.frag
        rendering/standard/shaders/line_thick.vert
        rendering/standard/shaders/line_thick_picking.vert
        rendering/standard/shaders/line_thick_picking.frag
        rendering/standard/shaders/marker_box.vert
        rendering/standard/shaders/marker_box.frag
        rendering/standard/shaders/marker_box_picking.vert
        rendering/standard/shaders/marker_box_picking.frag
        rendering/standard/shaders/text_billboard.vert
        rendering/standard/shaders/text_billboard.frag
        rendering/standard/shaders/mesh.vert
        rendering/standard/shaders/mesh_visual.frag
        rendering/standard/shaders/mesh_oit_accum.frag
        rendering/standard/shaders/mesh_oit_reveal.frag
        rendering/standard/shaders/mesh_picking.vert
        rendering/standard/shaders/mesh_picking.frag
        rendering/standard/shaders/mesh_color_mapping.vert
        rendering/standard/shaders/mesh_color_mapping.frag
        rendering/standard/shaders/mesh_instanced.vert
        rendering/standard/shaders/mesh_instanced_colors.vert
        rendering/standard/shaders/mesh_instanced_sorted.vert
        rendering/standard/shaders/mesh_instanced_picking.vert
        rendering/standard/shaders/mesh_wireframe.vert
        rendering/standard/shaders/mesh_wireframe.frag
        rendering/standard/shaders/mesh_wireframe_instanced.vert
        rendering/standard/shaders/mesh_wireframe_tri.vert
        rendering/standard/shaders/mesh_wireframe_tri_instanced.vert
)

if(OVITO_BUILD_BASIC)
    qt_add_shaders(Core "core_watermark_shaders"
        PRECOMPILE
        GLSL "450"
        HLSL "60"
        MSL "12"
        PREFIX "/ovito/core"
        FILES
            rendering/standard/shaders/watermark.vert
            rendering/standard/shaders/watermark.frag
    )
endif()

# Disable Qt AUTOMOC processing of the header OvitoClass.h.
# This is necessary, because the header contains the definition of the OVITO_CLASS macro, which
# is added to the AUTOMOC_MACRO_NAMES list.
SET_PROPERTY(SOURCE oo/OvitoClass PROPERTY SKIP_AUTOMOC ON)

IF(NOT EMSCRIPTEN)
    # Qt Network module is used by the Core module, except in the wasm build.
    OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Network REQUIRED)
    TARGET_LINK_LIBRARIES(Core PUBLIC Qt6::Network)
ENDIF()

# Use zlib library if available to support loading/saving gzip compressed files.
IF(ZLIB_FOUND)
    TARGET_SOURCES(Core PRIVATE utilities/io/gzdevice/GzipIODevice.cpp)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_ZLIB_SUPPORT)
    TARGET_LINK_LIBRARIES(Core PRIVATE ZLIB::ZLIB)

    # Also activate support for zstd compressed files.
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_ZSTD_SUPPORT)
    IF(OVITO_BUILD_CONDA)
        # In Conda builds, we expect zstd to be available via the Conda environment. We link the dynamic library there.
        OVITO_FIND_PACKAGE(zstd REQUIRED)
        TARGET_LINK_LIBRARIES(Core PRIVATE zstd::libzstd)
    ELSE()
        # In non-Conda builds, we use the statically built zstd library from our 3rdparty/ directory.
        # We use the static library version to avoid an extra deployment step when building the release packages of OVITO.
        # Note: The static lib must have been built with the -fPIC option.
        TARGET_LINK_LIBRARIES(Core PRIVATE zstd::libzstd_static)
    ENDIF()
    # Additionally, build zstd's zlib-wrapper source files, because the OVITO code uses zstd through this compatibility layer.
    TARGET_INCLUDE_DIRECTORIES(Core PRIVATE "$<BUILD_INTERFACE:${OVITO_SOURCE_BASE_DIR}/src/3rdparty/zstd/zlibWrapper>")
    SET(ZSTD_ZLIBWRAPPER_SOURCES
        "${OVITO_SOURCE_BASE_DIR}/src/3rdparty/zstd/zlibWrapper/zstd_zlibwrapper.c"
        "${OVITO_SOURCE_BASE_DIR}/src/3rdparty/zstd/zlibWrapper/gzclose.c"
        "${OVITO_SOURCE_BASE_DIR}/src/3rdparty/zstd/zlibWrapper/gzlib.c"
        "${OVITO_SOURCE_BASE_DIR}/src/3rdparty/zstd/zlibWrapper/gzread.c"
        "${OVITO_SOURCE_BASE_DIR}/src/3rdparty/zstd/zlibWrapper/gzwrite.c"
    )
    # Define STDC for the zstd zlibWrapper source files, because newer zlib versions (1.3+) no longer define
    # this macro in zconf.h, but the zlibWrapper code in gzguts.h still relies on it to include standard headers.
    SET_SOURCE_FILES_PROPERTIES(${ZSTD_ZLIBWRAPPER_SOURCES} PROPERTIES COMPILE_DEFINITIONS "STDC")
    TARGET_SOURCES(Core PRIVATE ${ZSTD_ZLIBWRAPPER_SOURCES})
ELSE()
    MESSAGE(WARNING "zlib library not found. OVITO will be built without I/O support for gzip compressed files.")
ENDIF()

# Link to Libssh.
IF(OVITO_BUILD_SSH_CLIENT)
    TARGET_SOURCES(Core PRIVATE
        utilities/io/ssh/libssh/LibsshConnection.cpp
        utilities/io/ssh/libssh/SshChannel.cpp
        utilities/io/ssh/libssh/ProcessChannel.cpp
        utilities/io/ssh/libssh/ScpChannel.cpp
        utilities/io/ssh/libssh/LsChannel.cpp
    )
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_SSH_CLIENT)
    TARGET_LINK_LIBRARIES(Core PRIVATE ssh)
ENDIF()

# Link to video encoding lib.
IF(FFMPEG_FOUND)
    TARGET_SOURCES(Core PRIVATE utilities/io/video/InternalVideoEncoderBackend.cpp)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_VIDEO_OUTPUT_SUPPORT)
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS)
    TARGET_LINK_LIBRARIES(Core PRIVATE "${FFMPEG_LIBRARIES}")
    # Use regular include (not system include) for FFmpeg
    TARGET_INCLUDE_DIRECTORIES(Core BEFORE PRIVATE "${FFMPEG_INCLUDE_DIRS}")
ELSE()
    MESSAGE(WARNING "ffmpeg video encoding library not found. OVITO will be built without support for internal video encoding.")
ENDIF()

# Define the OVITO_DEBUG macro in debug builds.
TARGET_COMPILE_DEFINITIONS(Core PUBLIC "$<$<CONFIG:Debug>:OVITO_DEBUG>")
# Define QT_NO_DEBUG_OUTPUT in release builds.
TARGET_COMPILE_DEFINITIONS(Core PUBLIC "$<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT=1>")

# Enable SYCL in source code.
IF(OVITO_USE_SYCL STREQUAL AdaptiveCpp)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_SYCL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_SYCL_ACPP)
ELSEIF(OVITO_USE_SYCL STREQUAL DPC++)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_SYCL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_SYCL_DPCPP)
ENDIF()

# Enable CUDA code.
IF(OVITO_USE_CUDA)
    # Tell source code that the use of CUDA is enabled.
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_CUDA)
    # Deploy CUDA runtime library with the application.
    IF(NOT OVITO_BUILD_CONDA)
        OVITO_FIND_PACKAGE(CUDAToolkit REQUIRED)
        GET_TARGET_PROPERTY(CUDA_LIBRARY_LOCATION CUDA::cudart LOCATION)
        IF(WIN32)
            # Derive path of DLL from .lib location.
            GET_FILENAME_COMPONENT(CUDA_LIBRARY_PATH "${CUDA_LIBRARY_LOCATION}" PATH)
            IF(CUDAToolkit_VERSION VERSION_GREATER_EQUAL 13.0)
                SET(CUDA_LIBRARY_LOCATION "${CUDA_LIBRARY_PATH}/../../bin/x64/cudart64_13.dll")
            ELSE()
                SET(CUDA_LIBRARY_LOCATION "${CUDA_LIBRARY_PATH}/../../bin/cudart64_12.dll")
            ENDIF()
        ENDIF()
        OVITO_INSTALL_SHARED_LIB("${CUDA_LIBRARY_LOCATION}")
    ENDIF()
ENDIF()

# Compute the relative path from the OVITO executable to where the plugin libraries get installed.
FILE(RELATIVE_PATH OVITO_PLUGINS_RELATIVE_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_PLUGINS_DIRECTORY}")
TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_PLUGINS_RELATIVE_PATH=\"${OVITO_PLUGINS_RELATIVE_PATH}\"")

# Compute the relative path from the OVITO executable to the software Vulkan driver that the
# redistributable Linux package ships as a fallback for systems without a Vulkan driver.
# See cmake/Prerequisites.cmake and RenderThread::prepareVulkanEnvironment().
IF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)
    FILE(RELATIVE_PATH OVITO_VULKAN_FALLBACK_RELATIVE_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_LIBRARY_DIRECTORY}/vulkan-fallback")
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_VULKAN_FALLBACK_RELATIVE_PATH=\"${OVITO_VULKAN_FALLBACK_RELATIVE_PATH}\"")
ENDIF()

# Every Linux build calls dlopen()/dladdr(): RenderThread inspects the Vulkan installation to
# tell the user which package is missing, and Application::bundledResourceDirPath() asks the
# linker where the shared library lives. Older glibc versions keep those functions in a separate
# libdl, which has to be linked explicitly.
IF(LINUX)
    TARGET_LINK_LIBRARIES(Core PRIVATE ${CMAKE_DL_LIBS})
ENDIF()

# Ship a font family with the Linux packages. A minimal Linux installation may provide no fonts
# and no /etc/fonts directory at all, in which case fontconfig has nothing to find and Qt renders
# no text at all. On such a system Application::createQtApplication() generates a fontconfig
# configuration pointing at these files; on a system that has its own font configuration, they
# are simply never used.
#
# This lives here rather than in src/main/, where it used to, because that directory is only
# added to the build when OVITO_BUILD_APP is ON and the PyPI wheel is built with it OFF.
IF(LINUX AND (OVITO_REDISTRIBUTABLE_PACKAGE OR OVITO_BUILD_PYPI))
    # Search for the files rather than expecting one directory. The distributions do not agree
    # on where the DejaVu packages put them: AlmaLinux 8 uses /usr/share/fonts/dejavu/, while
    # RHEL 10 -- the base of the aarch64 build image -- gives each package a directory of its
    # own. Hard-coding the first of those silently shipped no fonts at all on aarch64.
    SET(OVITO_FALLBACK_FONT_DIR "/usr/share/fonts" CACHE PATH
        "Directory tree searched for the font files that are shipped with the Linux packages.")
    FILE(GLOB_RECURSE _available_fonts LIST_DIRECTORIES FALSE "${OVITO_FALLBACK_FONT_DIR}/DejaVu*.ttf")
    FOREACH(_font IN ITEMS
            DejaVuSans.ttf DejaVuSans-Bold.ttf DejaVuSans-Oblique.ttf DejaVuSans-BoldOblique.ttf
            DejaVuSerif.ttf DejaVuSerif-Bold.ttf DejaVuSansMono.ttf DejaVuSansMono-Bold.ttf)
        SET(_font_path "")
        FOREACH(_candidate IN LISTS _available_fonts)
            GET_FILENAME_COMPONENT(_candidate_name "${_candidate}" NAME)
            IF(_candidate_name STREQUAL _font)
                SET(_font_path "${_candidate}")
                BREAK()
            ENDIF()
        ENDFOREACH()
        IF(_font_path)
            INSTALL(FILES "${_font_path}" DESTINATION "${OVITO_RELATIVE_SHARE_DIRECTORY}/fonts/")
        ELSE()
            MESSAGE(WARNING "Fallback font not found below ${OVITO_FALLBACK_FONT_DIR}: ${_font}. "
                "The package will render no text on systems that have no fonts installed.")
        ENDIF()
    ENDFOREACH()
    UNSET(_available_fonts)
    UNSET(_font_path)
    UNSET(_candidate_name)
ENDIF()

# Tell Core where those font files end up.
#
# The path has to be relative to whatever Application::bundledResourceDirPath() returns at
# runtime, and that is not the same directory in both packages: for the program package it is
# the directory of the executable, but inside the wheel the running executable is the Python
# interpreter, so the location is taken from the shared library instead. Anchoring both on
# OVITO_BINARY_DIRECTORY sent the wheel looking for ovito/plugins/fonts/ while the files were
# installed to ovito/fonts/, and the fallback then did nothing at all.
IF(LINUX AND (OVITO_REDISTRIBUTABLE_PACKAGE OR OVITO_BUILD_PYPI))
    IF(OVITO_BUILD_PYPI)
        SET(_ovito_resource_anchor "${OVITO_PLUGINS_DIRECTORY}")
    ELSE()
        SET(_ovito_resource_anchor "${OVITO_BINARY_DIRECTORY}")
    ENDIF()
    FILE(RELATIVE_PATH OVITO_FALLBACK_FONTS_RELATIVE_PATH "${_ovito_resource_anchor}" "${OVITO_SHARE_DIRECTORY}/fonts")
    MESSAGE(STATUS "Fallback fonts will be looked up at '${OVITO_FALLBACK_FONTS_RELATIVE_PATH}' relative to ${_ovito_resource_anchor}")
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_FALLBACK_FONTS_RELATIVE_PATH=\"${OVITO_FALLBACK_FONTS_RELATIVE_PATH}\"")
    UNSET(_ovito_resource_anchor)
ENDIF()

IF(NOT OVITO_BUILD_CONDA)
    # Compute the relative path from the OVITO executable to where the Python module layer gets installed.
    FILE(RELATIVE_PATH OVITO_PYTHON_LAYER_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_PYTHON_DIRECTORY}")
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_PYTHON_LAYER_PATH=\"${OVITO_PYTHON_LAYER_PATH}\"")
ELSE()
    # Compute the relative path from the Conda prefix to where the Python module layer gets installed.
    FILE(RELATIVE_PATH OVITO_PYTHON_LAYER_PATH "$ENV{PREFIX}" "$ENV{SP_DIR}")
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_PYTHON_LAYER_PATH=\"${OVITO_PYTHON_LAYER_PATH}\"")
ENDIF()

# Link to the pthread library on Linux platform.
IF(LINUX)
    SET(THREADS_PREFER_PTHREAD_FLAG ON)
    OVITO_FIND_PACKAGE(Threads REQUIRED)
    TARGET_LINK_LIBRARIES(Core PUBLIC Threads::Threads)
ENDIF()

IF(NOT OVITO_DOUBLE_PRECISION_FP)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "FLOATTYPE_FLOAT")
ENDIF()
IF(OVITO_BUILD_APPSTORE_VERSION)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_APPSTORE_VERSION")
ENDIF()
IF(OVITO_BUILD_MONOLITHIC)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_MONOLITHIC")
ENDIF()
IF(OVITO_DISABLE_THREADING)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_DISABLE_THREADING")
ENDIF()
IF(OVITO_BUILD_PROFESSIONAL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_PROFESSIONAL")
ENDIF()
IF(OVITO_BUILD_PYPI)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_PYPI")
ENDIF()
IF(OVITO_BUILD_BASIC)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_BASIC")
ENDIF()
IF(OVITO_BUILD_CONDA)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_CONDA")
ENDIF()
IF(OVITO_IS_DEVELOPMENT_BUILD)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_IS_DEVELOPMENT_BUILD")
ENDIF()
IF(EMSCRIPTEN)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_DISABLE_QSETTINGS")
ENDIF()

# Inject program release version into the source code.
SET_PROPERTY(SOURCE app/Application.cpp app/StandaloneApplication.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OVITO_VERSION_MAJOR=${OVITO_VERSION_MAJOR}"
    "OVITO_VERSION_MINOR=${OVITO_VERSION_MINOR}"
    "OVITO_VERSION_REVISION=${OVITO_VERSION_REVISION}"
    "OVITO_VERSION_STRING=\"${OVITO_VERSION_STRING}\""
    "OVITO_APPLICATION_NAME=\"${OVITO_APPLICATION_NAME}\"")
SET_PROPERTY(SOURCE utilities/io/SaveStream.cpp utilities/io/LoadStream.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OVITO_FILE_FORMAT_VERSION=${OVITO_FILE_FORMAT_VERSION}")

# Run clang-tidy to check C++ code and perform static analysis.
IF(OVITO_CLANG_TIDY_CMD)
    SET_TARGET_PROPERTIES(Core PROPERTIES CXX_CLANG_TIDY "${OVITO_CLANG_TIDY_CMD}")
ENDIF()

# Install ffmpeg video encoding libraries.
IF(FFMPEG_FOUND AND NOT EMSCRIPTEN)
    FOREACH(lib ${FFMPEG_LIBRARIES})
        GET_FILENAME_COMPONENT(base_name "${lib}" NAME_WE)
        GET_FILENAME_COMPONENT(lib_path "${lib}" PATH)
        IF(WIN32 AND NOT OVITO_BUILD_CONDA)
            FILE(GLOB lib_dll "${lib_path}/${base_name}-*.dll")
            IF(NOT lib_dll)
                MESSAGE(FATAL_ERROR "Could not find corresponding DLL for ffmpeg library '${lib}' in same directory.")
            ENDIF()
            FOREACH(dll ${lib_dll})
                OVITO_INSTALL_SHARED_LIB("${dll}")
            ENDFOREACH()
        ELSEIF(LINUX AND (OVITO_REDISTRIBUTABLE_PACKAGE OR OVITO_BUILD_PYPI))
            FILE(GLOB lib_versions "${lib}*")
            FOREACH(lib_version ${lib_versions})
                OVITO_INSTALL_SHARED_LIB("${lib_version}")
            ENDFOREACH()
        ENDIF()
    ENDFOREACH()
ENDIF()

IF(ZLIB_FOUND AND NOT EMSCRIPTEN)
    IF(WIN32 AND NOT OVITO_BUILD_CONDA)
        # Deploy the zlib DLL.
        GET_TARGET_PROPERTY(ZLIB_LIBRARY_LOCATION ZLIB::ZLIB LOCATION)
        GET_FILENAME_COMPONENT(ZLIB_LIBRARY_PATH "${ZLIB_LIBRARY_LOCATION}" PATH)
        OVITO_INSTALL_SHARED_LIB("${ZLIB_LIBRARY_PATH}/../bin/zlib.dll")
    ENDIF()
ENDIF()

# Deploy the libssh library and its dependencies.
IF(OVITO_BUILD_SSH_CLIENT AND NOT EMSCRIPTEN AND NOT OVITO_BUILD_CONDA)

    # Find OpenSSL 3.0 or newer, needed by libssh.
    OVITO_FIND_PACKAGE(OpenSSL 3.0 REQUIRED COMPONENTS Crypto SSL)

    # Get the location of the libssh and OpenSSL libraries.
    GET_TARGET_PROPERTY(LIBSSH_LIBRARY_LOCATION ssh LOCATION)
    GET_TARGET_PROPERTY(OPENSSL_CRYPTO_LIBRARY_LOCATION OpenSSL::Crypto LOCATION)
    GET_TARGET_PROPERTY(OPENSSL_SSL_LIBRARY_LOCATION OpenSSL::SSL LOCATION)

    IF(WIN32)
        # Install libssh.dll
        GET_FILENAME_COMPONENT(LIBSSH_LIBRARY_DIR "${LIBSSH_LIBRARY_LOCATION}" PATH)
        GET_FILENAME_COMPONENT(LIBSSH_LIBRARY_NAME_WE "${LIBSSH_LIBRARY_LOCATION}" NAME_WE)
        OVITO_INSTALL_SHARED_LIB("${LIBSSH_LIBRARY_DIR}/../bin/${LIBSSH_LIBRARY_NAME_WE}.dll")

        # Also install OpenSSL DLLs needed by libssh.
        GET_FILENAME_COMPONENT(OPENSSL_CRYPTO_LIBRARY_DIR "${OPENSSL_CRYPTO_LIBRARY_LOCATION}" PATH)
        GET_FILENAME_COMPONENT(OPENSSL_SSL_LIBRARY_DIR "${OPENSSL_SSL_LIBRARY_LOCATION}" PATH)
        OVITO_INSTALL_SHARED_LIB("${OPENSSL_CRYPTO_LIBRARY_DIR}/../bin/libcrypto-3-x64.dll")
        OVITO_INSTALL_SHARED_LIB("${OPENSSL_SSL_LIBRARY_DIR}/../bin/libssl-3-x64.dll")
    ELSEIF(APPLE)
        # Since OVITO 3.16, we no longer ship PKCS#11 support. Use the external OpenSSH connection method instead, which is fully compatible with smartcard authentication.

        # Install the OpenSSL PKCS#11 provider module needed by libssh.
        #GET_FILENAME_COMPONENT(OPENSSL_LIBRARY_DIR "${OPENSSL_CRYPTO_LIBRARY_LOCATION}" PATH)
        #INSTALL(FILES "${OPENSSL_LIBRARY_DIR}/ossl-modules/pkcs11.dylib" DESTINATION "${OVITO_RELATIVE_PLUGINS_DIRECTORY}/ossl-modules/")

        # Additionally, copy the provider modules directory into the build directory to make it available during testing.
        #FILE(MAKE_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}/ossl-modules")
        #FILE(COPY "${OPENSSL_LIBRARY_DIR}/ossl-modules/pkcs11.dylib" DESTINATION "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}/ossl-modules/")
    ELSEIF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)
        # Install libssh.so
        OVITO_INSTALL_SHARED_LIB("${LIBSSH_LIBRARY_LOCATION}")

        # Install OpenSSL (libcrypto.so and libssl.so) needed by libssh.
        #
        # EXACT, because find_package(OpenSSL) reports the unversioned development symlink
        # /usr/lib64/libcrypto.so, and the search for siblings of that name matches every
        # soversion in the directory. The base image has OpenSSL 1.1 installed next to the
        # OpenSSL 3 that OVITO builds and uses, so the package shipped both -- 6 MB of
        # libcrypto.so.1.1 and libssl.so.1.1 that nothing in it references.
        OVITO_INSTALL_SHARED_LIB("${OPENSSL_CRYPTO_LIBRARY_LOCATION}" EXACT)
        OVITO_INSTALL_SHARED_LIB("${OPENSSL_SSL_LIBRARY_LOCATION}" EXACT)

        # Since OVITO 3.16, we no longer ship PKCS#11 support. Use the external OpenSSH connection method instead, which is fully compatible with smartcard authentication.
        #IF(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        #    # Install the OpenSSL PKCS#11 provider module needed by libssh.
        #    GET_FILENAME_COMPONENT(OPENSSL_LIBRARY_DIR "${OPENSSL_CRYPTO_LIBRARY_LOCATION}" PATH)
        #    INSTALL(FILES "${OPENSSL_LIBRARY_DIR}/ossl-modules/pkcs11.so" DESTINATION "${OVITO_RELATIVE_PLUGINS_DIRECTORY}/ossl-modules/")
        #ENDIF()
    ENDIF()
ENDIF()
