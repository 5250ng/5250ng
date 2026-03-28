@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: 5250ng Build Script for Windows
:: ============================================================================
:: Usage: build.bat [options]
::   Options:
::     clean       - Clean build directory before building
::     release     - Build in Release mode (default is Debug)
::     test        - Run tests after building
::     package     - Create deployment package
::     help        - Show this help message
:: ============================================================================

set BUILD_TYPE=Debug
set RUN_TESTS=0
set CLEAN_BUILD=0
set CREATE_PACKAGE=0
set BUILD_DIR=build

:: Parse command line arguments
:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="clean" set CLEAN_BUILD=1
if /i "%~1"=="release" set BUILD_TYPE=Release
if /i "%~1"=="test" set RUN_TESTS=1
if /i "%~1"=="package" (
    set CREATE_PACKAGE=1
    set BUILD_TYPE=Release
)
if /i "%~1"=="help" goto :show_help
shift
goto :parse_args
:done_args

echo.
echo ============================================================
echo  5250ng Build Script
echo  Build Type: %BUILD_TYPE%
echo ============================================================
echo.

:: ----------------------------------------------------------------
:: Auto-detect Qt6 installation (MinGW and MSVC kits)
:: ----------------------------------------------------------------
if not defined QT_ROOT (
    for %%Q in (
        "C:\Qt\6.10.2\mingw_64"
        "C:\Qt\6.9.1\mingw_64"
        "C:\Qt\6.9.0\mingw_64"
        "C:\Qt\6.8.0\mingw_64"
        "C:\Qt\6.7.0\mingw_64"
        "C:\Qt\6.10.2\msvc2022_64"
        "C:\Qt\6.9.1\msvc2022_64"
        "C:\Qt\6.9.0\msvc2022_64"
        "C:\Qt\6.8.0\msvc2022_64"
        "C:\Qt\6.7.0\msvc2022_64"
        "C:\Qt\6.6.0\msvc2022_64"
        "C:\Qt\6.5.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.10.2\mingw_64"
        "%USERPROFILE%\Qt\6.9.1\mingw_64"
        "%USERPROFILE%\Qt\6.9.0\mingw_64"
        "%USERPROFILE%\Qt\6.8.0\mingw_64"
        "%USERPROFILE%\Qt\6.7.0\mingw_64"
        "%USERPROFILE%\Qt\6.10.2\msvc2022_64"
        "%USERPROFILE%\Qt\6.9.1\msvc2022_64"
        "%USERPROFILE%\Qt\6.9.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.8.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.7.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.6.0\msvc2022_64"
    ) do (
        if exist "%%~Q\lib\cmake\Qt6" (
            set "QT_ROOT=%%~Q"
            set "Qt6_DIR=%%~Q\lib\cmake\Qt6"
            echo [INFO] Found Qt6 at: %%~Q
            goto :qt_found
        )
    )
    echo [WARNING] Qt6 not found in common locations
    echo Please set QT_ROOT environment variable or install Qt6
    echo Example: set QT_ROOT=C:\Qt\6.10.2\mingw_64
)
:qt_found

:: Determine if this is a MinGW or MSVC kit
set "IS_MINGW=0"
if defined QT_ROOT (
    echo "%QT_ROOT%" | findstr /i "mingw" >nul 2>&1
    if !errorlevel!==0 set "IS_MINGW=1"
)

:: ----------------------------------------------------------------
:: Auto-detect Qt-bundled tools (CMake, Ninja, MinGW compiler)
:: ----------------------------------------------------------------

:: Derive Qt base dir (parent of version dir, e.g. C:\Qt)
if defined QT_ROOT (
    for %%R in ("%QT_ROOT%\..\..\Tools") do set "QT_TOOLS=%%~fR"
)

:: Add Qt-bundled CMake to PATH if system cmake is missing
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    if defined QT_TOOLS (
        if exist "%QT_TOOLS%\CMake_64\bin\cmake.exe" (
            set "PATH=%QT_TOOLS%\CMake_64\bin;%PATH%"
            echo [INFO] Using Qt-bundled CMake: %QT_TOOLS%\CMake_64\bin
        )
    )
)

:: Verify cmake is available
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found in PATH or Qt Tools
    echo Please install CMake and add it to your PATH
    exit /b 1
)

:: Add Qt-bundled Ninja to PATH if not already available
where ninja >nul 2>&1
if %errorlevel% neq 0 (
    if defined QT_TOOLS (
        if exist "%QT_TOOLS%\Ninja\ninja.exe" (
            set "PATH=%QT_TOOLS%\Ninja;%PATH%"
            echo [INFO] Using Qt-bundled Ninja: %QT_TOOLS%\Ninja
        )
    )
)

:: For MinGW kits, find the matching compiler in Qt Tools
set "MINGW_BIN="
if %IS_MINGW%==1 (
    if defined QT_TOOLS (
        for /d %%M in ("%QT_TOOLS%\mingw*_64") do (
            if exist "%%M\bin\g++.exe" (
                set "MINGW_BIN=%%M\bin"
                echo [INFO] Found MinGW compiler: %%M\bin
            )
        )
    )
    if defined MINGW_BIN (
        set "PATH=!MINGW_BIN!;%PATH%"
    ) else (
        echo [WARNING] MinGW compiler not found in Qt Tools
        echo Please ensure g++ is in your PATH
    )
)

:: ----------------------------------------------------------------
:: Auto-detect OpenSSL
:: ----------------------------------------------------------------
if not defined OPENSSL_ROOT_DIR (
    for %%O in (
        "C:\Program Files\OpenSSL-Win64"
        "C:\Program Files\OpenSSL"
        "C:\OpenSSL-Win64"
        "C:\OpenSSL"
    ) do (
        if exist "%%~O\include\openssl" (
            set "OPENSSL_ROOT_DIR=%%~O"
            echo [INFO] Found OpenSSL at: %%~O
            goto :openssl_found
        )
    )
    echo [WARNING] OpenSSL not found in common locations
    echo Please set OPENSSL_ROOT_DIR environment variable or install OpenSSL
)
:openssl_found

:: ----------------------------------------------------------------
:: For MinGW builds with MSVC-compiled OpenSSL, generate import libs
:: ----------------------------------------------------------------
set "OPENSSL_MINGW_DIR="
if %IS_MINGW%==1 if defined OPENSSL_ROOT_DIR (
    :: Check if OpenSSL has MinGW-compatible libs already
    if exist "%OPENSSL_ROOT_DIR%\lib\libcrypto.dll.a" (
        echo [INFO] Found MinGW-compatible OpenSSL libraries
    ) else if exist "%OPENSSL_ROOT_DIR%\bin\libcrypto-3-x64.dll" (
        :: Need to generate MinGW import libraries from MSVC DLLs
        set "OPENSSL_MINGW_DIR=%CD%\%BUILD_DIR%\openssl_mingw"
        if not exist "!OPENSSL_MINGW_DIR!\lib\libcrypto.dll.a" (
            echo [INFO] Generating MinGW import libraries for OpenSSL...
            mkdir "!OPENSSL_MINGW_DIR!\lib" 2>nul
            where gendef >nul 2>&1
            if !errorlevel!==0 (
                pushd "!OPENSSL_MINGW_DIR!\lib"
                gendef "%OPENSSL_ROOT_DIR%\bin\libcrypto-3-x64.dll" >nul 2>&1
                dlltool -d libcrypto-3-x64.def -l libcrypto.dll.a -D libcrypto-3-x64.dll 2>nul
                gendef "%OPENSSL_ROOT_DIR%\bin\libssl-3-x64.dll" >nul 2>&1
                dlltool -d libssl-3-x64.def -l libssl.dll.a -D libssl-3-x64.dll 2>nul
                del /q *.def 2>nul
                popd
                if exist "!OPENSSL_MINGW_DIR!\lib\libcrypto.dll.a" (
                    echo [INFO] MinGW import libraries generated successfully
                ) else (
                    echo [WARNING] Failed to generate MinGW import libraries
                    set "OPENSSL_MINGW_DIR="
                )
            ) else (
                echo [WARNING] gendef not found - cannot generate MinGW import libraries
                echo          Install MinGW gendef or use a MinGW-compiled OpenSSL
                set "OPENSSL_MINGW_DIR="
            )
        ) else (
            echo [INFO] Using cached MinGW import libraries for OpenSSL
        )
    )
)

:: ----------------------------------------------------------------
:: Clean build directory if requested
:: ----------------------------------------------------------------
if %CLEAN_BUILD%==1 (
    echo [INFO] Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

:: Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: ----------------------------------------------------------------
:: Configure with CMake
:: ----------------------------------------------------------------
echo.
echo [INFO] Configuring project with CMake...
echo.

set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if defined QT_ROOT (
    set CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_PREFIX_PATH="!QT_ROOT!"
)

:: OpenSSL configuration
if defined OPENSSL_MINGW_DIR (
    :: Use generated MinGW import libs, but real headers
    set CMAKE_ARGS=!CMAKE_ARGS! -DOPENSSL_ROOT_DIR="!OPENSSL_MINGW_DIR!"
    set CMAKE_ARGS=!CMAKE_ARGS! -DOPENSSL_INCLUDE_DIR="!OPENSSL_ROOT_DIR!\include"
) else if defined OPENSSL_ROOT_DIR (
    set CMAKE_ARGS=!CMAKE_ARGS! -DOPENSSL_ROOT_DIR="!OPENSSL_ROOT_DIR!"
)

:: Select CMake generator and compiler
if %IS_MINGW%==1 (
    :: MinGW build: prefer Ninja, set compiler explicitly
    where ninja >nul 2>&1
    if !errorlevel!==0 (
        set CMAKE_GENERATOR=-G "Ninja"
        echo [INFO] Using Ninja generator with MinGW
    ) else (
        set CMAKE_GENERATOR=-G "MinGW Makefiles"
        echo [INFO] Using MinGW Makefiles generator
    )
    if defined MINGW_BIN (
        set CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_C_COMPILER="!MINGW_BIN!\gcc.exe"
        set CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_CXX_COMPILER="!MINGW_BIN!\g++.exe"
    )
) else (
    :: MSVC build: prefer Ninja, fall back to Visual Studio
    where ninja >nul 2>&1
    if !errorlevel!==0 (
        set CMAKE_GENERATOR=-G "Ninja"
        echo [INFO] Using Ninja generator
    ) else (
        set CMAKE_GENERATOR=-G "Visual Studio 17 2022" -A x64
        echo [INFO] Using Visual Studio 2022 generator
    )
)

cmake -S . -B "%BUILD_DIR%" %CMAKE_GENERATOR% %CMAKE_ARGS%
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed
    exit /b 1
)

:: ----------------------------------------------------------------
:: Build
:: ----------------------------------------------------------------
echo.
echo [INFO] Building project...
echo.

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed
    exit /b 1
)

:: Determine executable path (Ninja puts it in bin\, MSVC in bin\Release\)
if exist "%BUILD_DIR%\bin\5250ng.exe" (
    set "EXE_PATH=%BUILD_DIR%\bin\5250ng.exe"
) else if exist "%BUILD_DIR%\bin\%BUILD_TYPE%\5250ng.exe" (
    set "EXE_PATH=%BUILD_DIR%\bin\%BUILD_TYPE%\5250ng.exe"
) else (
    set "EXE_PATH=%BUILD_DIR%\bin\5250ng.exe"
)

echo.
echo [SUCCESS] Build completed successfully
echo [INFO] Executable: %EXE_PATH%

:: ----------------------------------------------------------------
:: Run tests if requested
:: ----------------------------------------------------------------
if %RUN_TESTS%==1 (
    echo.
    echo [INFO] Running tests...
    echo.

    :: Add Qt bin to PATH so test executables can find DLLs
    if defined QT_ROOT set "PATH=%QT_ROOT%\bin;%PATH%"
    if defined OPENSSL_ROOT_DIR set "PATH=%OPENSSL_ROOT_DIR%\bin;%PATH%"

    pushd "%BUILD_DIR%"
    ctest -C %BUILD_TYPE% --output-on-failure -E "test_tn5250_connection"
    if %errorlevel% neq 0 (
        echo.
        echo [WARNING] Some tests failed
    ) else (
        echo.
        echo [SUCCESS] All tests passed
    )
    popd
)

:: ----------------------------------------------------------------
:: Create deployment package if requested
:: ----------------------------------------------------------------
if %CREATE_PACKAGE%==1 (
    echo.
    echo [INFO] Creating deployment package...

    set "DEPLOY_DIR=deploy\5250ng"
    if exist "deploy" rmdir /s /q "deploy"
    mkdir "!DEPLOY_DIR!"

    :: Copy executable
    copy "!EXE_PATH!" "!DEPLOY_DIR!\" >nul

    :: Run windeployqt to gather Qt dependencies
    if defined QT_ROOT (
        echo [INFO] Running windeployqt...
        "!QT_ROOT!\bin\windeployqt.exe" --release "!DEPLOY_DIR!\5250ng.exe"
        if !errorlevel! neq 0 (
            echo [WARNING] windeployqt failed - Qt DLLs may be missing
        )
    ) else (
        echo [WARNING] QT_ROOT not set - skipping windeployqt
        echo          You may need to manually copy Qt DLLs
    )

    :: Copy OpenSSL DLLs if available
    if defined OPENSSL_ROOT_DIR (
        if exist "!OPENSSL_ROOT_DIR!\bin\libcrypto-3-x64.dll" (
            copy "!OPENSSL_ROOT_DIR!\bin\libcrypto-3-x64.dll" "!DEPLOY_DIR!\" >nul
            copy "!OPENSSL_ROOT_DIR!\bin\libssl-3-x64.dll" "!DEPLOY_DIR!\" >nul
            echo [INFO] Copied OpenSSL DLLs
        )
    )

    :: Create zip archive
    set "ZIP_NAME=5250ng-windows-x64.zip"
    if exist "deploy\!ZIP_NAME!" del /q "deploy\!ZIP_NAME!"
    echo [INFO] Creating zip archive: deploy\!ZIP_NAME!
    powershell -NoProfile -Command "Compress-Archive -Path '!DEPLOY_DIR!\*' -DestinationPath 'deploy\!ZIP_NAME!' -Force"
    if !errorlevel!==0 (
        echo [SUCCESS] Zip archive created at: deploy\!ZIP_NAME!
    ) else (
        echo [WARNING] Failed to create zip archive
    )

    echo.
    echo [SUCCESS] Deployment package created at: !DEPLOY_DIR!
)

echo.
echo ============================================================
echo  Build Complete
echo ============================================================
goto :eof

:show_help
echo.
echo 5250ng Build Script for Windows
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   clean       Clean build directory before building
echo   release     Build in Release mode (default is Debug)
echo   test        Run tests after building
echo   package     Create deployment package (implies release)
echo   help        Show this help message
echo.
echo Examples:
echo   build.bat                    Build Debug version
echo   build.bat release            Build Release version
echo   build.bat clean release      Clean and build Release
echo   build.bat release test       Build Release and run tests
echo   build.bat package            Create distributable package
echo.
echo Environment Variables:
echo   QT_ROOT          Path to Qt6 kit (auto-detected)
echo                    Example: set QT_ROOT=C:\Qt\6.10.2\mingw_64
echo   OPENSSL_ROOT_DIR Path to OpenSSL installation (auto-detected)
echo.
echo Supported Toolchains:
echo   - MinGW (auto-detected from Qt Tools)
echo   - Visual Studio 2022 (MSVC)
echo.
echo Requirements:
echo   - Qt6 (Core, Widgets, Network) with MinGW or MSVC kit
echo   - OpenSSL (optional, for TLS support)
echo   - CMake 3.16+ and Ninja (bundled with Qt, or install separately)
echo.
goto :eof
