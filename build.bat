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

:: Check for required tools
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found in PATH
    echo Please install CMake and add it to your PATH
    exit /b 1
)

:: Try to find Qt6
if not defined Qt6_DIR (
    :: Common Qt installation paths on Windows
    for %%Q in (
        "C:\Qt\6.8.0\msvc2022_64"
        "C:\Qt\6.7.0\msvc2022_64"
        "C:\Qt\6.6.0\msvc2022_64"
        "C:\Qt\6.5.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.8.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.7.0\msvc2022_64"
        "%USERPROFILE%\Qt\6.6.0\msvc2022_64"
    ) do (
        if exist "%%~Q\lib\cmake\Qt6" (
            set "Qt6_DIR=%%~Q\lib\cmake\Qt6"
            set "QT_ROOT=%%~Q"
            echo [INFO] Found Qt6 at: %%~Q
            goto :qt_found
        )
    )
    echo [WARNING] Qt6 not found in common locations
    echo Please set Qt6_DIR environment variable or install Qt6
    echo Example: set Qt6_DIR=C:\Qt\6.8.0\msvc2022_64\lib\cmake\Qt6
)
:qt_found

:: Try to find OpenSSL
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

:: Clean build directory if requested
if %CLEAN_BUILD%==1 (
    echo [INFO] Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

:: Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: Configure with CMake
echo.
echo [INFO] Configuring project with CMake...
echo.

set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if defined Qt6_DIR (
    set CMAKE_ARGS=%CMAKE_ARGS% -DQt6_DIR="%Qt6_DIR%"
)

if defined OPENSSL_ROOT_DIR (
    set CMAKE_ARGS=%CMAKE_ARGS% -DOPENSSL_ROOT_DIR="%OPENSSL_ROOT_DIR%"
)

:: Prefer Ninja if available, fall back to Visual Studio
where ninja >nul 2>&1
if %errorlevel%==0 (
    set CMAKE_GENERATOR=-G "Ninja"
    echo [INFO] Using Ninja generator
) else (
    set CMAKE_GENERATOR=-G "Visual Studio 17 2022" -A x64
    echo [INFO] Using Visual Studio 2022 generator
)

cmake -S . -B "%BUILD_DIR%" %CMAKE_GENERATOR% %CMAKE_ARGS%
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed
    exit /b 1
)

:: Build
echo.
echo [INFO] Building project...
echo.

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [SUCCESS] Build completed successfully
echo [INFO] Executable: %BUILD_DIR%\bin\%BUILD_TYPE%\tn5250.exe

:: Run tests if requested
if %RUN_TESTS%==1 (
    echo.
    echo [INFO] Running tests...
    echo.
    cd "%BUILD_DIR%"
    ctest -C %BUILD_TYPE% --output-on-failure
    if %errorlevel% neq 0 (
        echo.
        echo [WARNING] Some tests failed
    ) else (
        echo.
        echo [SUCCESS] All tests passed
    )
    cd ..
)

:: Create deployment package if requested
if %CREATE_PACKAGE%==1 (
    echo.
    echo [INFO] Creating deployment package...
    
    set "DEPLOY_DIR=deploy\5250ng"
    if exist "deploy" rmdir /s /q "deploy"
    mkdir "%DEPLOY_DIR%"
    
    :: Copy executable
    copy "%BUILD_DIR%\bin\%BUILD_TYPE%\tn5250.exe" "%DEPLOY_DIR%\" >nul
    
    :: Run windeployqt to gather Qt dependencies
    if defined QT_ROOT (
        echo [INFO] Running windeployqt...
        "%QT_ROOT%\bin\windeployqt.exe" --release "%DEPLOY_DIR%\tn5250.exe"
        if %errorlevel% neq 0 (
            echo [WARNING] windeployqt failed - Qt DLLs may be missing
        )
    ) else (
        echo [WARNING] QT_ROOT not set - skipping windeployqt
        echo          You may need to manually copy Qt DLLs
    )
    
    :: Copy OpenSSL DLLs if available
    if defined OPENSSL_ROOT_DIR (
        if exist "%OPENSSL_ROOT_DIR%\bin\libcrypto-3-x64.dll" (
            copy "%OPENSSL_ROOT_DIR%\bin\libcrypto-3-x64.dll" "%DEPLOY_DIR%\" >nul
            copy "%OPENSSL_ROOT_DIR%\bin\libssl-3-x64.dll" "%DEPLOY_DIR%\" >nul
            echo [INFO] Copied OpenSSL DLLs
        )
    )
    
    echo.
    echo [SUCCESS] Deployment package created at: %DEPLOY_DIR%
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
echo   Qt6_DIR          Path to Qt6 CMake config (auto-detected)
echo   OPENSSL_ROOT_DIR Path to OpenSSL installation (auto-detected)
echo.
echo Requirements:
echo   - CMake 3.16 or later
echo   - Visual Studio 2022 (or Ninja + MSVC)
echo   - Qt6 (Core, Widgets, Network)
echo   - OpenSSL
echo.
goto :eof

