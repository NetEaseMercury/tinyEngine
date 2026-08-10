# Builds RenderDoc's capture core (renderdoc.dll) from the submodule at
# thirdParty/renderdoc_src, and deploys it as a Vulkan implicit layer next to the
# engine DLL.
#
# Notes / constraints discovered while wiring this up:
#  * RenderDoc's own CMakeLists refuses to run on Windows ("CMake is not needed on
#    Windows, just open and build renderdoc.sln"), so we drive MSBuild directly on
#    the vcxproj instead of add_subdirectory().
#  * Building renderdoc.vcxproj standalone needs SolutionDir passed explicitly,
#    otherwise its $(SolutionDir)\util\*.props imports resolve to nothing.
#  * The checked-in projects target PlatformToolset v140 (VS2015); we retarget to
#    the toolset of the detected VS via PlatformToolset.
#  * Only the capture core is built. qrenderdoc (the UI) needs Qt + Python and is
#    intentionally NOT built; replay uses an existing qrenderdoc.exe (system
#    install, or RENDERDOC_QRENDERDOC_EXE).

function(te_build_renderdoc)
    set(RDOC_SRC "${CMAKE_SOURCE_DIR}/thirdParty/renderdoc_src")
    set(RDOC_PROJ "${RDOC_SRC}/renderdoc/renderdoc.vcxproj")

    if(NOT EXISTS "${RDOC_PROJ}")
        message(FATAL_ERROR
            "TE_BUILD_RENDERDOC=ON but the RenderDoc submodule is missing.\n"
            "Run: git submodule update --init --recursive thirdParty/renderdoc_src")
    endif()

    # Check out the revision requested in thirdParty/versions.cfg so the built
    # core matches the qrenderdoc used for replay (.rdc format / target-control
    # protocol change between versions).
    find_package(Git QUIET)
    if(GIT_FOUND AND RENDERDOC_VERSION)
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --always
            WORKING_DIRECTORY "${RDOC_SRC}"
            OUTPUT_VARIABLE _rdoc_current OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(NOT _rdoc_current STREQUAL RENDERDOC_VERSION)
            message(STATUS "RenderDoc: checking out ${RENDERDOC_VERSION} (was ${_rdoc_current})")
            execute_process(COMMAND "${GIT_EXECUTABLE}" checkout --quiet "${RENDERDOC_VERSION}"
                WORKING_DIRECTORY "${RDOC_SRC}" RESULT_VARIABLE _co_res)
            if(NOT _co_res EQUAL 0)
                message(WARNING "RenderDoc: failed to check out ${RENDERDOC_VERSION}; building whatever is checked out.")
            endif()
        endif()
    endif()

    # Locate MSBuild via vswhere (the VS installer ships it at a fixed path).
    set(_vswhere "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
    set(MSBUILD_EXE "")
    if(EXISTS "${_vswhere}")
        execute_process(COMMAND "${_vswhere}" -latest -requires Microsoft.Component.MSBuild
                                -find "MSBuild/**/Bin/MSBuild.exe"
            OUTPUT_VARIABLE MSBUILD_EXE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        # vswhere may return several matches; take the first.
        if(MSBUILD_EXE)
            string(REPLACE "\n" ";" _msbuild_list "${MSBUILD_EXE}")
            list(GET _msbuild_list 0 MSBUILD_EXE)
            string(STRIP "${MSBUILD_EXE}" MSBUILD_EXE)
        endif()
    endif()
    if(NOT MSBUILD_EXE OR NOT EXISTS "${MSBUILD_EXE}")
        message(FATAL_ERROR "TE_BUILD_RENDERDOC=ON but MSBuild.exe could not be located via vswhere.")
    endif()
    message(STATUS "RenderDoc: MSBuild at ${MSBUILD_EXE}")

    # Map our config to RenderDoc's (it uses Development / Release).
    set(_rdoc_cfg "$<IF:$<CONFIG:Debug>,Development,Release>")
    set(_rdoc_out "${RDOC_SRC}/x64/$<IF:$<CONFIG:Debug>,Development,Release>/renderdoc.dll")

    # Retarget from the checked-in v140 toolset to the current VS toolset.
    set(_toolset "v143")
    if(MSVC_TOOLSET_VERSION)
        set(_toolset "v${MSVC_TOOLSET_VERSION}")
    endif()

    add_custom_target(renderdoc_core ALL
        COMMAND "${MSBUILD_EXE}" "${RDOC_PROJ}"
                "-p:Configuration=${_rdoc_cfg}"
                -p:Platform=x64
                "-p:SolutionDir=${RDOC_SRC}\\"
                "-p:PlatformToolset=${_toolset}"
                -m -v:minimal -nologo
        BYPRODUCTS "${_rdoc_out}"
        COMMENT "Building RenderDoc capture core (${RENDERDOC_VERSION}, toolset ${_toolset})"
        VERBATIM)

    # Deploy the freshly built core + our layer manifest next to the engine DLL.
    add_custom_command(TARGET tinyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_rdoc_out}" "$<TARGET_FILE_DIR:tinyEngine>/renderdoc.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/thirdParty/renderdoc/renderdoc.json"
            "$<TARGET_FILE_DIR:tinyEngine>/renderdoc.json"
        COMMENT "Deploy self-built RenderDoc capture layer"
        VERBATIM)
    add_dependencies(tinyEngine renderdoc_core)
endfunction()
