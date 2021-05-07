REM Input
set PLATFORM=x64
REM set CONFIGURATION=Release
REM if "%CONFIGURATION" == "debug" set CONFIGURATION=Debug
set PDFium_BRANCH=cheftin

echo Config=%CONFIGURATION%

set DepotTools_URL=http://cxan.kdr2.com/misc/depot_tools-f9eef45d.zip
set DepotTools_DIR=%CD%/depot_tools

set PDFium_URL=https://github.com/PaodingAI/PDFium.git
set PDFium_SOURCE_DIR=%CD%\pdfium

set PDFium_BUILD_DIR=%PDFium_SOURCE_DIR%\out-%CONFIGURATION%
set PDFium_CI_DIR=%PDFium_SOURCE_DIR%\ci-build
set PDFium_CMAKE_CONFIG=%PDFium_CI_DIR%\PDFiumConfig.cmake
set PDFium_ARGS=%PDFium_CI_DIR%\args\windows.args.gn

REM Output
set PDFium_STAGING_DIR=%CD%\staging-%CONFIGURATION%
set PDFium_INCLUDE_DIR=%PDFium_STAGING_DIR%\include
set PDFium_BIN_DIR=%PDFium_STAGING_DIR%\%PLATFORM%\bin
set PDFium_LIB_DIR=%PDFium_STAGING_DIR%\%PLATFORM%\lib
set PDFium_ARTIFACT=%CD%\pdfium-windows-%PLATFORM%.zip
if "%CONFIGURATION%"=="Debug" set PDFium_ARTIFACT=%CD%\pdfium-windows-%PLATFORM%-debug.zip

call curl -fsSL -o depot_tools.zip %DepotTools_URL%
call 7z -bd x depot_tools.zip -o.

set PATH=%DepotTools_DIR%;%PATH%
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

call gclient config --unmanaged %PDFium_URL% --name=pdfium
call gclient sync

REM Prepare directories
echo on
mkdir %PDFium_BUILD_DIR%
mkdir %PDFium_STAGING_DIR%
mkdir %PDFium_BIN_DIR%
mkdir %PDFium_LIB_DIR%

REM Checkout to target branch
echo on
cd %PDFium_SOURCE_DIR%
call git checkout %PDFium_BRANCH%
cd ..
call gclient sync
call python -m pip install pywin32

echo on
cd %PDFium_SOURCE_DIR%
call git checkout -- .

copy %PDFium_ARGS% %PDFium_BUILD_DIR%\args.gn
if "%CONFIGURATION%"=="Release" echo is_debug=false >> %PDFium_BUILD_DIR%\args.gn
if "%CONFIGURATION%"=="Debug" echo is_debug=true >> %PDFium_BUILD_DIR%\args.gn
if "%PLATFORM%"=="x86" echo target_cpu="x86" >> %PDFium_BUILD_DIR%\args.gn

call gn gen %PDFium_BUILD_DIR%

call ninja -C %PDFium_BUILD_DIR% pdfium
if %errorlevel% neq 0 exit /b %errorlevel%

copy %PDFium_CMAKE_CONFIG% %PDFium_STAGING_DIR%
copy %PDFium_SOURCE_DIR%\LICENSE %PDFium_STAGING_DIR%
REM rd /S /Q %PDFium_INCLUDE_DIR%
xcopy /S /Y %PDFium_SOURCE_DIR%\public %PDFium_INCLUDE_DIR%\
del %PDFium_INCLUDE_DIR%\DEPS
del %PDFium_INCLUDE_DIR%\README
del %PDFium_INCLUDE_DIR%\PRESUBMIT.py
move %PDFium_BUILD_DIR%\pdfium.dll.lib %PDFium_LIB_DIR%
move %PDFium_BUILD_DIR%\pdfium.dll %PDFium_BIN_DIR%
if "%CONFIGURATION%"=="Debug" move %PDFium_BUILD_DIR%\pdfium.dll.pdb %PDFium_BIN_DIR%

cd %PDFium_STAGING_DIR%
call 7z a %PDFium_ARTIFACT% *

C:\cygwin64\bin\bash %PDFium_CI_DIR%\upload-win.sh %PDFium_SOURCE_DIR% %PDFium_ARTIFACT% %PLATFORM% %CONFIGURATION%
