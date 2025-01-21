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

# imgui/assimp不是标准cmake工程，需要手动配置头文件
file(GLOB_RECURSE IMGUI_SRCS thirdParty/imgui/*.cpp thirdParty/imgui/*.h)
file(GLOB_RECURSE assimp_SRCS thirdParty/assimp/*.cpp thirdParty/assimp/*.h thirdParty  assimp/*.hpp)
file(GLOB_RECURSE spirvReflect_SRCS thirdParty/spirv-reflect/*.c thirdParty/spirv-reflect/*.cpp thirdParty/spirv-reflect/*.h thirdParty/spirv-reflect/*.hpp)
file(GLOB_RECURSE stbImage_SRCS thirdParty/stb_image/*.h)
file(GLOB_RECURSE SOURCE src/scene/*.cpp src/scene/*.hpp src/scene/*.h src/IMGUI/*.hpp src/IMGUI/*.cpp src/engine/*.h src/engine/*.hpp src/engine/*.cpp src/*.h src/*.cpp)
# 将源代码添加到此项目的可执行文件。
add_executable(${PROJECT_NAME}
    ${SOURCE}
    ${IMGUI_SRCS}
    ${assimp_SRCS}
    ${spirvReflect_SRCS}
    ${stbImage_SRCS}
    )

target_include_directories(${PROJECT_NAME} PUBLIC
                thirdParty/glfw/include
                thirdParty/stb
                thirdParty/tinyobjloader
                thirdParty/imgui
                thirdParty/assimp
                thirdParty/stb_image
                thirdParty/spirv-reflect
        )


# 获取当前二进制目录到RUNTIME_OUTPUT_DIRECTORY这个变量处，也可以讲 ${CMAKE_BINARY_DIR}替换为"/path/to/your/directory"
set_target_properties(tinyEngine PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# 获取运行时Directory
get_target_property(TARGET_DIR tinyEngine RUNTIME_OUTPUT_DIRECTORY)
# 添加预处理命令
add_definitions(-DGLEW_STATIC)

# 拷贝资源
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/res")
set(SOURCE_LIBS "${CMAKE_SOURCE_DIR}/")
set(SOURCE_FILE_ASSIMP  "${CMAKE_SOURCE_DIR}/thirdParty/libs/assimp-vc140-mt.lib" "${CMAKE_SOURCE_DIR}/thirdParty/libs/assimp-vc140-mt.exp" "${CMAKE_SOURCE_DIR}/thirdParty/libs/assimp-vc140-mt.dll")
set(SOURCE_FILE_MEMORYALLOCATOR "${CMAKE_SOURCE_DIR}/thirdParty/libs/VulkanMemoryAllocator.lib")
set(DESTINATION_DIR "${CMAKE_BINARY_DIR}")
file(COPY ${SOURCE_DIR} ${SOURCE_FILE_ASSIMP} ${SOURCE_FILE_MEMORYALLOCATOR} DESTINATION ${DESTINATION_DIR})

link_directories("${CMAKE_SOURCE_DIR}/thirdParty/libs/")
target_link_libraries(${PROJECT_NAME}
        Vulkan::Vulkan
        glfw ${GLFW_LIBRARIES}
        glm::glm    
        assimp-vc140-mt
        )
target_compile_definitions(${PROJECT_NAME} PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
        GLFW_INCLUDE_VULKAN
        TINYOBJLOADER_IMPLEMENTATION
        )


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tinyEngine PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
