echo on

rem This script is used by AppVeyor to build the project.

if %bits% equ 64 goto :eof
if %prg% neq npackdcl goto :eof

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL

set onecmd="%npackd_cl%\ncl.exe" path -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,20)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p org.7-zip.SevenZIP -r [9,20)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set SEVENZIP=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p exeproxy -r [0.2,1)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set EXEPROXY=%%x

if %bits% equ 64 goto bits64

set QT=C:\NpackdSymlinks\com.nokia.QtDev-i686-w64-Npackd-Release-5.5
set PACKAGE=com.googlecode.windows-package-manager.Npackd
set mingw_libs=i686-w64-mingw32

set onecmd="%npackd_cl%\ncl.exe" path -p mingw-w64-i686-sjlj-posix -v 4.9.2
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p quazip-dev-i686-w64-static -v 0.7.1
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p z-dev-i686-w64_sjlj_posix_4.9.2-static -v 1.2.11
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set zlib=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p drmingw -v 0.7.7
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x

goto start

:bits64

set QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.5
set PACKAGE=com.googlecode.windows-package-manager.Npackd64
set mingw_libs=x86_64-w64-mingw32

set onecmd="%npackd_cl%\ncl.exe" path -p mingw-w64-x86_64-seh-posix -v 4.9.2
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p quazip-dev-x86_64-w64-static -v 0.7.1
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p z-dev-x86_64-w64_seh_posix_4.9.2-static -v 1.2.11
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set zlib=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p drmingw64 -v 0.7.7
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x

goto start

:start
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


