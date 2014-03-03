set /p version=< version.txt

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.nokia.QtDev-x86_64-w64-Npackd-Release" "--versions=[4.8.2, 4.8.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set qt=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW6464" "--versions=[4.7.2, 4.7.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

rem set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.8.2, 4.8.2]"
rem for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

rem set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=mingw-w64-i686-sjlj-posix" "--versions=[4.8.2, 4.8.2]"
rem for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw32=%%x

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

"%mingw%\bin\mingw32-make.exe" -j 3
if %errorlevel% neq 0 goto error

:error

