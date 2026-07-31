@echo off
rem ============================================================================
rem build_all.bat - one-shot build for tinyEngine (C++ DLL) + tinyEditor (C# WPF)
rem
rem This script lives in build_scripts/; the project root is one level up.
rem
rem Steps:
rem   1. Generate build\tinyEditor.sln when missing. Open that sln from the
rem      build/ directory in Visual Studio; the build itself targets the
rem      .csproj directly and does not need the sln.
rem      The sln is written from an embedded template because `dotnet sln add`
rem      cannot handle project paths outside the sln directory (".." segments
rem      are rejected by current .NET SDKs).
rem   2. CMake configure (preset x64-debug, idempotent; runs from the project
rem      root where CMakePresets.json lives).
rem   3. CMake build: engine DLL first, then the C# Editor (via the tinyEditor
rem      wrapper target in editor/CMakeLists.txt).
rem
rem Outputs:
rem   - Engine DLL + res/ : out\build\x64-debug\
rem   - Editor binaries   : out\editor\bin\Debug\net8.0-windows\
rem
rem Note: close any running tinyEditor instance before building, otherwise the
rem output files are locked and the build fails.
rem ============================================================================
setlocal
set "PROJ_ROOT=%~dp0.."
set "BUILD_DIR=%PROJ_ROOT%\build"

echo [build_all] Checking C# solution file...
if not exist "%BUILD_DIR%\tinyEditor.sln" (
    echo [build_all] build\tinyEditor.sln not found, generating...
    if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
    (
        echo Microsoft Visual Studio Solution File, Format Version 12.00
        echo # Visual Studio Version 17
        echo VisualStudioVersion = 17.0.31903.59
        echo MinimumVisualStudioVersion = 10.0.40219.1
        echo Project("{9A19103F-16F7-4668-BE54-9A1E7A4F7556}"^) = "tinyEditor", "..\editor\tinyEditor\tinyEditor.csproj", "{3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}"
        echo EndProject
        echo Global
        echo 	GlobalSection(SolutionConfigurationPlatforms^) = preSolution
        echo 		Debug^|Any CPU = Debug^|Any CPU
        echo 		Release^|Any CPU = Release^|Any CPU
        echo 	EndGlobalSection
        echo 	GlobalSection(ProjectConfigurationPlatforms^) = postSolution
        echo 		{3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}.Debug^|Any CPU.ActiveCfg = Debug^|Any CPU
        echo 		{3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}.Debug^|Any CPU.Build.0 = Debug^|Any CPU
        echo 		{3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}.Release^|Any CPU.ActiveCfg = Release^|Any CPU
        echo 		{3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}.Release^|Any CPU.Build.0 = Release^|Any CPU
        echo 	EndGlobalSection
        echo EndGlobal
    ) > "%BUILD_DIR%\tinyEditor.sln"
    if errorlevel 1 exit /b 1
)

echo [build_all] Configuring CMake (x64-debug)...
pushd "%PROJ_ROOT%"
cmake --preset x64-debug
if errorlevel 1 (popd & exit /b 1)

echo [build_all] Building engine + editor...
cmake --build --preset x64-debug
if errorlevel 1 (popd & exit /b 1)
popd

echo [build_all] Done. Editor: out\editor\bin\Debug\net8.0-windows\tinyEditor.exe
endlocal
