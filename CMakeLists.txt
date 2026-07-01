# CMakeList.txt: tinyEngine 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
project (tinyEngine)
add_subdirectory(thirdParty/glfw)
add_subdirectory(thirdParty/glm)
# add Vulkan
find_package(Vulkan REQUIRED)

# imgui不是标准cmake工程，需要手动配置头文件
file(GLOB_RECURSE IMGUI_SRCS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/thirdParty/imgui/*.cpp")

# src 下源码：CONFIGURE_DEPENDS 在增删 .cpp 时会触发重新配置，避免链接缺文件
file(GLOB SOURCE CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME}
    ${SOURCE}
    ${IMGUI_SRCS}
    thirdParty/ImGuizmo/ImGuizmo.cpp
)

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


# 获取当前二进制目录到RUNTIME_OUTPUT_DIRECTORY这个变量处，也可以讲 ${CMAKE_BINARY_DIR}替换为"/path/to/your/directory"
set_target_properties(tinyEngine PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# 获取运行时Directory
get_target_property(TARGET_DIR tinyEngine RUNTIME_OUTPUT_DIRECTORY)
# 添加预处理命令
add_definitions(-DGLEW_STATIC)

# ── HLSL → SPIR-V via DXC (auto-download if missing) ────────────────────────
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DXC_ARCH x64)
else()
    set(DXC_ARCH x86)
endif()
set(DXC_DIR "${CMAKE_SOURCE_DIR}/thirdParty/dxc")
set(DXC_EXE "${DXC_DIR}/bin/${DXC_ARCH}/dxc.exe")
set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602.24/dxc_2026_05_27.zip")

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

add_custom_target(shaders DEPENDS ${SHADER_SPV_FILES})
add_dependencies(${PROJECT_NAME} shaders)

# 拷贝资源（configure 时一次 + 每次链接后同步）
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
        )
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tinyEngine PROPERTY CXX_STANDARD 20)
endif()


# TODO: 如有需要，请添加测试并安装目标。
