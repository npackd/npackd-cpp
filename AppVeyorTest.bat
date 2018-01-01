echo on

rem This script is used by AppVeyor to build the project.

if %bits% equ 64 goto :eof
if %prg% neq npackdcl goto :eof

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL

cd npackdcl
cd tests

mkdir build
if %errorlevel% neq 0 exit /b %errorlevel%

cd build

set path=%mingw%\bin;C:\Program Files (x86)\CMake\bin;%ai%\bin\x86;%sevenzip%
set qtdir=%qt:\=/%
set CMAKE_INCLUDE_PATH=%quazip%\quazip
set CMAKE_LIBRARY_PATH=%quazip%\quazip\release
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=..\install "-DZLIB_ROOT:PATH=%zlib%"
if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

..\install\tests -v2
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..\..\ftests

mkdir build
if %errorlevel% neq 0 exit /b %errorlevel%

cd build

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=..\install "-DZLIB_ROOT:PATH=%zlib%"
if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

..\install\ftests -v2
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof


