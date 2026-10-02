@echo off
setlocal EnableExtensions

rem ============================================================
rem Find the same WinLibs toolchain used for Assimp/engine.
rem ============================================================

set "TOOLBIN="

for %%P in (
    "F:\mingw64\mingw32\bin"
    "F:\mingw32\bin"
    "F:\winlibs\mingw32\bin"
    "F:\mingw64\bin"
    "F:\winlibs\mingw64\bin"
) do (
    if exist "%%~P\g++.exe" (
        set "TOOLBIN=%%~P"
        goto :tool_found
    )
)

for /f "delims=" %%G in ('where g++.exe 2^>nul') do (
    for %%D in ("%%G") do set "TOOLBIN=%%~dpD"
    goto :tool_found
)

:tool_found

if not defined TOOLBIN (
    echo G++ do WinLibs nao encontrado.
    pause
    exit /b 1
)

if "%TOOLBIN:~-1%"=="\" set "TOOLBIN=%TOOLBIN:~0,-1%"

set "PATH=%TOOLBIN%;%PATH%"

rem ============================================================
rem Assimp
rem ============================================================

if not defined ASSIMP_ROOT (
    if exist "F:\assimp\include\assimp\Importer.hpp" (
        set "ASSIMP_ROOT=F:\assimp"
    )
)

if not defined ASSIMP_ROOT (
    echo.
    echo ==========================================
    echo ASSIMP NAO ENCONTRADO
    echo ==========================================
    echo.
    echo Defina a pasta gerada pelo builder:
    echo.
    echo   set ASSIMP_ROOT=F:\caminho\assimp_i686_msvcrt_fbx
    echo.
    echo Ela precisa conter:
    echo   %%ASSIMP_ROOT%%\include\assimp\Importer.hpp
    echo   %%ASSIMP_ROOT%%\lib\libassimp*.dll.a
    echo   %%ASSIMP_ROOT%%\bin\*.dll
    echo.
    pause
    exit /b 1
)

if not exist "%ASSIMP_ROOT%\include\assimp\Importer.hpp" (
    echo Header do Assimp nao encontrado:
    echo   %ASSIMP_ROOT%\include\assimp\Importer.hpp
    pause
    exit /b 1
)

echo.
echo === COMPILADOR ===
g++ --version
echo.
echo === TOOLBIN ===
echo %TOOLBIN%
echo.
echo === ASSIMP ===
echo %ASSIMP_ROOT%
echo.

if not exist build mkdir build

g++ ^
  src\main.cpp ^
  src\engine.cpp ^
  src\gl33.cpp ^
  src\math3d.cpp ^
  src\glb_loader.cpp ^
  src\fbx_loader.cpp ^
  src\model_loader.cpp ^
  src\ui.cpp ^
  src\renderer.cpp ^
  src\raycast_vehicle.cpp ^
  -std=c++17 ^
  -O2 ^
  -Wall ^
  -Wextra ^
  -D_WIN32_WINNT=0x0601 ^
  -Isrc ^
  -I"%ASSIMP_ROOT%\include" ^
  -L"%ASSIMP_ROOT%\lib" ^
  -lassimp ^
  -lopengl32 ^
  -lgdi32 ^
  -luser32 ^
  -lcomdlg32 ^
  -lole32 ^
  -lwindowscodecs ^
  -luuid ^
  -o build\model_engine.exe

if errorlevel 1 (
    echo.
    echo ==========================
    echo ERRO NA COMPILACAO
    echo ==========================
    echo.
    pause
    exit /b 1
)

rem ============================================================
rem COPY ALL Assimp DLLs to the executable directory.
rem Do NOT move/delete them from ASSIMP_ROOT because future builds
rem still need the original runtime package.
rem ============================================================

echo.
echo Copiando DLLs do Assimp para build\ ...

if exist "%ASSIMP_ROOT%\bin" (
    for %%F in ("%ASSIMP_ROOT%\bin\*.dll") do (
        if exist "%%~fF" (
            echo   %%~nxF
            copy /Y "%%~fF" "build\" >nul
        )
    )
)

rem Some custom builds may leave the DLL in lib.
if exist "%ASSIMP_ROOT%\lib" (
    for %%F in ("%ASSIMP_ROOT%\lib\assimp*.dll") do (
        if exist "%%~fF" (
            echo   %%~nxF
            copy /Y "%%~fF" "build\" >nul
        )
    )
)

rem ============================================================
rem Copy the exact runtime DLLs from the same WinLibs toolchain.
rem This matters for i686-posix-dwarf builds.
rem ============================================================

for %%D in (
    libstdc++-6.dll
    libgcc_s_dw2-1.dll
    libwinpthread-1.dll
) do (
    if exist "%TOOLBIN%\%%D" (
        echo   %%D
        copy /Y "%TOOLBIN%\%%D" "build\" >nul
    )
)

echo.
echo ==========================
echo COMPILADO COM SUCESSO
echo ==========================
echo.
echo EXE:
echo   build\model_engine.exe
echo.
echo DLLs instaladas ao lado do EXE:
dir /b build\*.dll 2>nul
echo.

build\model_engine.exe
pause
