set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.nokia.QtDev-x86_64-w64-Npackd-Release" "--versions=[4.8.2, 4.8.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set qt=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW6464" "--versions=[4.7.2, 4.7.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 11)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 10)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set sevenzip=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.5, 0.5]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set quazip_path=%quazip%

mkdir build
mkdir build\64

set path=%mingw%\bin
cd build\64
"%qt%\qmake\qmake.exe" ..\..\wpmcpp.pro -r -spec win32-g++ CONFIG+=release
if %errorlevel% neq 0 goto error

"%mingw%\bin\mingw32-make.exe"
if %errorlevel% neq 0 goto error

"%qt%\bin\lrelease.exe" ..\..\wpmcpp.pro
if %errorlevel% neq 0 goto error

mkdir zip

copy ..\..\wpmcpp_es.qm zip\npackdg_es.qm
if %errorlevel% neq 0 goto error

copy ..\..\wpmcpp_ru.qm zip\npackdg_ru.qm
if %errorlevel% neq 0 goto error

copy ..\..\wpmcpp_fr.qm zip\npackdg_fr.qm
if %errorlevel% neq 0 goto error

copy ..\..\wpmcpp_de.qm zip\npackdg_de.qm
if %errorlevel% neq 0 goto error

cd ..\..

copy LICENSE.txt build\64\zip

copy CrystalIcons_LICENSE.txt build\64\zip
if %errorlevel% neq 0 goto error

copy build\64\release\wpmcpp.exe build\64\zip\npackdg.exe
if %errorlevel% neq 0 goto error

"%mingw%\bin\strip.exe" build\64\zip\npackdg.exe
if %errorlevel% neq 0 goto error

rem creating .zip
cd build\64\zip
"%sevenzip%\7z" a ..\Npackd64.zip *
if %errorlevel% neq 0 goto error
cd ..\..\..

rem creating .msi
"%ai%\bin\x86\AdvancedInstaller.com" /build wpmcpp64.aip
if %errorlevel% neq 0 goto error

:error

