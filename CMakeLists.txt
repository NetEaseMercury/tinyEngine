# CMakeLists.txt: tinyEngine CMake project. Source files and project-specific
# logic are defined here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers when supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
project (tinyEngine)
add_subdirectory(thirdParty/glfw)
add_subdirectory(thirdParty/glm)
# add Vulkan
find_package(Vulkan REQUIRED)

# In Editor mode the engine is built as a DLL, driven by the C# Editor
# (editor/) through the C ABI.
# Sources are listed explicitly (no GLOB): IMGUIManager.cpp (the ImGui/ImGuizmo
# UI) has left the main path; it is kept on disk as a reference for a future
# viewport-overlay port and is not part of the build.
set(ENGINE_SOURCES
    src/BufferManager.cpp
    src/CommandManager.cpp
    src/DescriptorManager.cpp
    src/DebugCommand.cpp
    src/EngineRuntime.cpp
    src/FramebufferManager.cpp
    src/MaterialAssetLoader.cpp
    src/MaterialManager.cpp
    src/PickSystem.cpp
    src/PipelineManager.cpp
    src/Presenter.cpp
    src/Renderer.cpp
    src/RenderPassManager.cpp
    src/SceneManager.cpp
    src/SwapChain.cpp
    src/TextureManager.cpp
    src/TinyEngineDebug.cpp
    src/VulkanContext.cpp
    src/camera.cpp
    src/tinyengine_api.cpp
)

add_library(${PROJECT_NAME} SHARED ${ENGINE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
                thirdParty/glfw/include
                thirdParty/stb
                thirdParty/tinyobjloader
                thirdParty/imgui
                thirdParty/stb_image
                thirdParty/ImGuizmo
                thirdParty/renderdoc
                thirdParty/json/include
                thirdParty/cgltf
        )


# The DLL and import library go to ${CMAKE_BINARY_DIR} (VS multi-config
# generators append a <CONFIG> subdirectory by default; pinned per-config here
# so tinyEngine.dll always lands in out/build/<preset>/, matching the res/ and
# SPIR-V output directories for the C# Editor to copy directly).
set_target_properties(tinyEngine PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
                      RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}
                      RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}
                      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
                      ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib
                      ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
                      ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib
                      ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/lib)
# Get the runtime output directory
get_target_property(TARGET_DIR tinyEngine RUNTIME_OUTPUT_DIRECTORY)
# Add preprocessor definitions
add_definitions(-DGLEW_STATIC)

# ── Third-party versions (single source of truth: thirdParty/versions.cfg) ───
include(${CMAKE_SOURCE_DIR}/cmake/ReadDepsVersions.cmake)
te_read_deps_versions("${CMAKE_SOURCE_DIR}/thirdParty/versions.cfg")
message(STATUS "Dep versions: RenderDoc=${RENDERDOC_VERSION} DXC=${DXC_VERSION}")

# ── HLSL → SPIR-V via DXC (auto-download if missing) ────────────────────────
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DXC_ARCH x64)
else()
    set(DXC_ARCH x86)
endif()
set(DXC_DIR "${CMAKE_SOURCE_DIR}/thirdParty/dxc")
set(DXC_EXE "${DXC_DIR}/bin/${DXC_ARCH}/dxc.exe")
set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/${DXC_VERSION}/${DXC_PACKAGE}")

if(NOT EXISTS "${DXC_EXE}")
    set(DXC_ZIP "${DXC_DIR}/dxc.zip")
    if(NOT EXISTS "${DXC_ZIP}")
        message(STATUS "Downloading DXC from ${DXC_URL} ...")
        file(DOWNLOAD "${DXC_URL}" "${DXC_ZIP}" STATUS downloadStatus TIMEOUT 120)
        list(GET downloadStatus 0 statusCode)
        if(NOT statusCode EQUAL 0)
            list(GET downloadStatus 1 errorMsg)
            message(FATAL_ERROR "DXC download failed: ${errorMsg}")
        endif()
    endif()
    message(STATUS "Extracting DXC to ${DXC_DIR} ...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${DXC_ZIP}"
        WORKING_DIRECTORY "${DXC_DIR}")
    file(REMOVE "${DXC_ZIP}")
endif()
set(DXC_EXECUTABLE "${DXC_EXE}")
message(STATUS "DXC: ${DXC_EXECUTABLE}")

# ── RenderDoc capture core (built from submodule when requested) ─────────────
# OFF by default: building RenderDoc takes several minutes, and day-to-day work
# does not need it (the engine falls back to a system-installed RenderDoc).
# Turn on with: cmake --preset x64-debug -DTE_BUILD_RENDERDOC=ON
option(TE_BUILD_RENDERDOC "Build RenderDoc's capture core from thirdParty/renderdoc_src" OFF)

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/res/shaders")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/res/shaders")
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

function(dxc_compile SPV_NAME PROFILE HLSL_SRC)
    set(SPV_PATH "${SHADER_OUT_DIR}/${SPV_NAME}")
    add_custom_command(
        OUTPUT "${SPV_PATH}"
        COMMAND ${DXC_EXECUTABLE} -spirv -T ${PROFILE} -E main
                "${HLSL_SRC}" -Fo "${SPV_PATH}" -O3
        DEPENDS "${HLSL_SRC}"
        COMMENT "DXC ${HLSL_SRC} → ${SPV_NAME}"
    )
    set(SHADER_SPV_FILES "${SHADER_SPV_FILES};${SPV_PATH}" PARENT_SCOPE)
endfunction()

dxc_compile(vert.spv       vs_6_0 "${SHADER_SRC_DIR}/shader.vert.hlsl")
dxc_compile(frag.spv       ps_6_0 "${SHADER_SRC_DIR}/shader.frag.hlsl")
dxc_compile(box_vert.spv   vs_6_0 "${SHADER_SRC_DIR}/box_instanced.vert.hlsl")
dxc_compile(box.spv        ps_6_0 "${SHADER_SRC_DIR}/box.frag.hlsl")
dxc_compile(pick_vert.spv  vs_6_0 "${SHADER_SRC_DIR}/pick.vert.hlsl")
dxc_compile(pick_frag.spv  ps_6_0 "${SHADER_SRC_DIR}/pick.frag.hlsl")
dxc_compile(utility_vert.spv    vs_6_0 "${SHADER_SRC_DIR}/utility.vert.hlsl")
dxc_compile(solid_color.spv     ps_6_0 "${SHADER_SRC_DIR}/solid_color.frag.hlsl")

add_custom_target(shaders DEPENDS ${SHADER_SPV_FILES})
add_dependencies(${PROJECT_NAME} shaders)

# Copy resources (once at configure time + synced after every link)
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/res")
set(DESTINATION_DIR "${CMAKE_BINARY_DIR}")
file(COPY ${SOURCE_DIR} DESTINATION ${DESTINATION_DIR})
add_custom_command(TARGET tinyEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:tinyEngine>/res"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tinyEngine>/res/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/thirdParty/tinyobjloader/models/no_material.obj"
        "$<TARGET_FILE_DIR:tinyEngine>/res/models/no_material.obj"
    COMMENT "Sync res/ next to tinyEngine executable"
)

# RenderDoc capture layer deployment.
#  * TE_BUILD_RENDERDOC=ON  : build the core from the submodule and deploy it.
#  * OFF (default)          : deploy a prebuilt renderdoc.dll from thirdParty/
#                             renderdoc/ if one happens to be there; otherwise the
#                             engine just uses a system-installed RenderDoc.
# The deployed copy acts as a fallback layer, selected only when no system
# RenderDoc is registered (see TinyEngineDebug.cpp) — this avoids the dual-DLL
# situation where TriggerCapture flips a flag in one instance while the layer
# hooks run in another.
if(TE_BUILD_RENDERDOC)
    include(${CMAKE_SOURCE_DIR}/cmake/BuildRenderDoc.cmake)
    te_build_renderdoc()
elseif(EXISTS "${CMAKE_SOURCE_DIR}/thirdParty/renderdoc/renderdoc.dll")
    add_custom_command(TARGET tinyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/thirdParty/renderdoc/renderdoc.dll"
            "$<TARGET_FILE_DIR:tinyEngine>/renderdoc.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/thirdParty/renderdoc/renderdoc.json"
            "$<TARGET_FILE_DIR:tinyEngine>/renderdoc.json"
        COMMENT "Deploy prebuilt RenderDoc capture layer (fallback)")
endif()

target_link_libraries(${PROJECT_NAME}
        Vulkan::Vulkan
        glfw ${GLFW_LIBRARIES}
        glm::glm
        )

target_compile_definitions(${PROJECT_NAME} PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
        GLFW_INCLUDE_VULKAN
        STB_IMAGE_IMPLEMENTATION
        TINYOBJLOADER_IMPLEMENTATION
        TINYENGINE_EXPORTS
        )

# Sources are UTF-8; force MSVC to read them as UTF-8 regardless of the system
# code page.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tinyEngine PROPERTY CXX_STANDARD 20)
endif()


# C# WPF Editor (dotnet build wrapper target; DEPENDS tinyEngine guarantees
# DLL first, Editor second)
add_subdirectory(editor)

# TODO: Add tests and install targets if needed.
