@echo off
REM build_all.bat — build the C++ core, run the smoke test, and build the
REM Qt6 frontend on Windows (MSVC). Requires CMake on PATH and a Visual
REM Studio C++ toolchain. For the Qt frontend, set CMAKE_PREFIX_PATH to
REM your Qt 6 installation before running this script:
REM   set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64
setlocal enableextensions enabledelayedexpansion

set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%

echo ==^> Building C++ core
if not exist "%ROOT%\core\build" mkdir "%ROOT%\core\build"
pushd "%ROOT%\core\build"
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (popd & exit /b 1)
cmake --build . --config Release
if errorlevel 1 (popd & exit /b 1)

echo ==^> Running C++ smoke test
cmake --build . --target tlm_core_smoke --config Release
if errorlevel 1 (popd & exit /b 1)
".\Release\tlm_core_smoke.exe"
popd

echo.
echo ==^> Building Qt6 frontend
if not exist "%ROOT%\frontend_qt\build" mkdir "%ROOT%\frontend_qt\build"
pushd "%ROOT%\frontend_qt\build"
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    popd
    echo    ^(Qt6 build skipped — CMake could not find Qt6.^)
    goto :pyhint
)
cmake --build . --config Release
if errorlevel 1 (popd & echo    ^(Qt6 build failed.^) & goto :pyhint)
echo    -^> %ROOT%\frontend_qt\build\Release\tlm_qt.exe
popd

:pyhint
echo.
echo ==^> Python frontend:
echo     pip install -r %ROOT%\frontend_python\requirements.txt
echo     python %ROOT%\frontend_python\app_pyside6.py
echo.
echo ==^> Avalonia frontend:
echo     cd %ROOT%\frontend_avalonia\TlmAvalonia ^&^& dotnet run -c Release

endlocal
