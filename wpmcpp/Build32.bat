set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.nokia.QtDev-i686-w64-Npackd-Release" "--versions=[4.8.2, 4.8.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set qt=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW64" "--versions=[4.7.2, 4.7.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 11)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 10)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set sevenzip=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=quazip-dev-i686-w64-static" "--versions=[0.5, 0.5]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set quazip_path=%quazip%

mkdir build
mkdir build\32

set path=%mingw%\bin
cd build\32
"%qt%\qmake\qmake.exe" ..\..\wpmcpp.pro -r -spec win32-g++ CONFIG+=release
if %errorlevel% neq 0 goto error

"%mingw%\bin\mingw32-make.exe"
if %errorlevel% neq 0 goto error

cd ..\..

mkdir build\32\zip

copy LICENSE.txt build\32\zip

copy CrystalIcons_LICENSE.txt build\32\zip
if %errorlevel% neq 0 goto error

copy build\32\release\wpmcpp.exe build\32\zip\Npackdg.exe
if %errorlevel% neq 0 goto error

rem creating .zip
cd build\32\zip
"%sevenzip%\7z" a ..\Npackd32.zip *
if %errorlevel% neq 0 goto error
cd ..\..\..

rem creating .msi
"%ai%\bin\x86\AdvancedInstaller.com" /build wpmcpp.aip
if %errorlevel% neq 0 goto error

:error

