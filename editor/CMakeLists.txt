# CMake wrapper target for the C# WPF Editor.
#
# CMake's native CSharp language support is stuck in the old .NET Framework
# era and cannot handle SDK-style projects (net8.0-windows + WPF XAML
# compilation), so a custom target drives dotnet build here, with
# DEPENDS tinyEngine enforcing the order: engine DLL first, then the Editor
# (the csproj SyncEngineRuntime target copies the engine DLL + res/ into the
# Editor output directory).
#
# The build targets the .csproj directly instead of the .sln, so the repository
# does not need to track a solution file (build_all.bat regenerates one on
# demand for opening the project in Visual Studio).
#
# EngineOutDir is passed as an MSBuild global property so the csproj no longer
# hardcodes the engine build directory (no csproj changes needed when
# switching preset/generator).

find_program(DOTNET_EXE dotnet)

if (NOT DOTNET_EXE)
    message(WARNING "dotnet SDK not found - tinyEditor target disabled")
    return()
endif()

add_custom_target(tinyEditor ALL
    COMMAND ${DOTNET_EXE} build "${CMAKE_CURRENT_SOURCE_DIR}/tinyEditor/tinyEditor.csproj"
            --configuration $<CONFIG>
            -p:EngineOutDir=$<TARGET_FILE_DIR:tinyEngine>
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building C# Editor (tinyEditor.csproj)"
    VERBATIM
    DEPENDS tinyEngine
)
