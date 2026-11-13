@echo off
setlocal
REM scratch.bat — build+run the multi-env physics scaling benchmark (scratch space, pre-authorized).
REM Uses CLion's bundled MinGW + CMake. Release -O3 mirrors the wrapper build so per-tick physics
REM is byte-identical to the wrapper's vendored submodule copy.

set "CMAKE=C:\Program Files\JetBrains\CLion 2025.3.6.2\bin\cmake\win\x64\bin\cmake.exe"
set "MINGW=C:\Program Files\JetBrains\CLion 2025.3.6.2\bin\mingw\bin"
set "PATH=%MINGW%;%PATH%"

"%CMAKE%" -S scratch -B scratch/build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%MINGW%/g++.exe" -DCMAKE_C_COMPILER="%MINGW%/gcc.exe" -DCMAKE_MAKE_PROGRAM="%MINGW%/mingw32-make.exe"
if errorlevel 1 (echo CONFIGURE FAILED & exit /b 1)

"%CMAKE%" --build scratch/build --config Release -j
if errorlevel 1 (echo BUILD FAILED & exit /b 1)

echo === RUN ===
scratch\build\scratch.exe %*
