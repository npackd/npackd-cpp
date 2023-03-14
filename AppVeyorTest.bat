echo on

rem This script is used by AppVeyor to build the project.

if %bits% equ 64 goto :eof
if %prg% neq npackdcl goto :eof

SET NPACKD_CL=C:\Program Files\NpackdCL

set onecmd="%npackd_cl%\ncl.exe" path -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,20)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p org.7-zip.SevenZIP -r [9,20)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set SEVENZIP=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p exeproxy -r [0.2,1)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set EXEPROXY=%%x

if %bits% equ 64 goto bits64

set QT=C:/msys64/mingw32/qt5-static
set mingw_libs=i686-w64-mingw32
set mingw=C:\msys64\mingw32

set onecmd="%npackd_cl%\ncl.exe" path -p quazip-dev-i686-w64_dw2_posix_7.2-qt_5.9.2-static -v 0.7.3
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p drmingw -v 0.7.7
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x

goto start

set path=C:\msys64\mingw64\bin;%ai%\bin\x86;%sevenzip%

cd npackdcl\install

tests -v2
if %errorlevel% neq 0 exit /b %errorlevel%

rem 40 minutes timeout for *all* tests
set QTEST_FUNCTION_TIMEOUT=2400000
ftests -v2
if %errorlevel% neq 0 exit /b %errorlevel%
