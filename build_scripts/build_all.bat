@echo off
rem ============================================================================
rem build_all.bat - one-shot build for tinyEngine (C++ DLL) + tinyEditor (C# WPF)
rem
rem This script lives in build_scripts/; the project root is one level up.
rem
rem Steps:
rem   1. CMake configure (preset x64-debug, idempotent; runs from the project
rem      root where CMakePresets.json lives). This generates the C++ vcxproj at
rem      out\build\x64-debug\tinyEngine.vcxproj which the sln references.
rem   2. Generate build\tinyEditor.sln when missing. The sln contains BOTH the
rem      C# tinyEditor.csproj AND the generated C++ tinyEngine.vcxproj so a
rem      single VS instance can edit, build, and mixed-debug both sides. Open
rem      the sln in Visual Studio; the daily build itself targets the .csproj
rem      directly (BeforeBuild hook rebuilds the DLL) and does not need the sln.
rem      The sln is written from an embedded template because `dotnet sln add`
rem      cannot handle project paths outside the sln directory (".." segments
rem      are rejected by current .NET SDKs) and it also cannot register
rem      .vcxproj files.
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
setlocal enabledelayedexpansion
set "PROJ_ROOT=%~dp0.."
set "BUILD_DIR=%PROJ_ROOT%\build"
set "ENGINE_VCXPROJ=%PROJ_ROOT%\out\build\x64-debug\tinyEngine.vcxproj"

echo [build_all] Configuring CMake (x64-debug)...
pushd "%PROJ_ROOT%"
cmake --preset x64-debug
if errorlevel 1 (popd & exit /b 1)
popd

if not exist "%ENGINE_VCXPROJ%" (
    echo [build_all] ERROR: expected %ENGINE_VCXPROJ% after configure.
    exit /b 1
)

echo [build_all] Checking C# solution file...
if not exist "%BUILD_DIR%\tinyEditor.sln" (
    echo [build_all] build\tinyEditor.sln not found, generating...
    if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

    rem Extract the (deterministic) GUID CMake assigned to tinyEngine.vcxproj so
    rem the sln can reference it. Format in the vcxproj: <ProjectGuid>{GUID}</ProjectGuid>.
    set "ENGINE_GUID="
    for /f "tokens=2 delims={}" %%G in ('findstr /R /C:"<ProjectGuid>{" "%ENGINE_VCXPROJ%"') do (
        set "ENGINE_GUID={%%G}"
    )
    if not defined ENGINE_GUID (
        echo [build_all] ERROR: could not read ProjectGuid from %ENGINE_VCXPROJ%.
        exit /b 1
    )
    echo [build_all]   Editor csproj GUID : {3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}
    echo [build_all]   Engine vcxproj GUID: !ENGINE_GUID!

    rem  Layout of the mixed solution:
    rem   * tinyEditor (C#)   Project type {9A19103F-...} (SDK-style csproj)
    rem   * tinyEngine (C++)  Project type {8BC9CEB8-...} (vcxproj)
    rem   * ProjectDependencies makes VS build tinyEngine before tinyEditor.
    rem   * Configuration mapping: sln's "Debug|Any CPU" maps to
    rem       - csproj  Debug|Any CPU
    rem       - vcxproj Debug|x64        (native platforms must be x64/Win32/ARM64)
    (
        echo Microsoft Visual Studio Solution File, Format Version 12.00
        echo # Visual Studio Version 17
        echo VisualStudioVersion = 17.0.31903.59
        echo MinimumVisualStudioVersion = 10.0.40219.1
        echo Project("{9A19103F-16F7-4668-BE54-9A1E7A4F7556}"^) = "tinyEditor", "..\editor\tinyEditor\tinyEditor.csproj", "{3E7A1B2C-9D4F-4A5E-8B6C-1F2D3E4A5B6C}"
        echo 	ProjectSection(ProjectDependencies^) = postProject
        echo 		!ENGINE_GUID! = !ENGINE_GUID!
        echo 	EndProjectSection
        echo EndProject
        echo Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"^) = "tinyEngine", "..\out\build\x64-debug\tinyEngine.vcxproj", "!ENGINE_GUID!"
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
        echo 		!ENGINE_GUID!.Debug^|Any CPU.ActiveCfg = Debug^|x64
        echo 		!ENGINE_GUID!.Debug^|Any CPU.Build.0 = Debug^|x64
        echo 		!ENGINE_GUID!.Release^|Any CPU.ActiveCfg = Release^|x64
        echo 		!ENGINE_GUID!.Release^|Any CPU.Build.0 = Release^|x64
        echo 	EndGlobalSection
        echo 	GlobalSection(SolutionProperties^) = preSolution
        echo 		HideSolutionNode = FALSE
        echo 	EndGlobalSection
    ) > "%BUILD_DIR%\tinyEditor.sln"
    if errorlevel 1 exit /b 1
    echo EndGlobal>> "%BUILD_DIR%\tinyEditor.sln"
)

echo [build_all] Building engine + editor...
pushd "%PROJ_ROOT%"
cmake --build --preset x64-debug
if errorlevel 1 (popd & exit /b 1)
popd

echo [build_all] Done. Editor: out\editor\bin\Debug\net8.0-windows\tinyEditor.exe
endlocal
