rem clone the source code repository
rem set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.selenic.mercurial.Mercurial64" "--versions=[2, 3)"
rem for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mercurial=%%x
rem "%mercurial%\hg.exe" clone https://code.google.com/p/windows-package-manager.npackd-cpp/ a
rem if %errorlevel% neq 0 exit /b %errorlevel%

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.nokia.QtDev-i686-w64-Npackd-Release" "--versions=[4.8.2, 4.8.2)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set qt=%%x
if %errorlevel% neq 0 exit /b %errorlevel%

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.nokia.QtDev-x86_64-w64-Npackd-Release" "--versions=[4.8.2, 4.8.2)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set qt64=%%x
if %errorlevel% neq 0 exit /b %errorlevel%

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW64" "--versions=[4.7.2, 4.7.2)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x
if %errorlevel% neq 0 exit /b %errorlevel%

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW6464" "--versions=[4.7.2, 4.7.2)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw64=%%x
if %errorlevel% neq 0 exit /b %errorlevel%

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 11)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x
if %errorlevel% neq 0 exit /b %errorlevel%

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP64" "--versions=[9, 10)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set sevenzip=%%x
if %errorlevel% neq 0 exit /b %errorlevel%

rem set quazip_path=C:/Users/t/projects/library-builder/QuaZIP-static-i686-w32-0.5

rmdir /S /Q build

mkdir build
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir build\release32
if %errorlevel% neq 0 exit /b %errorlevel%

set path=%qt_path%\mingw\bin
cd build\release32
"%qt_path%\Desktop\Qt\4.7.3\mingw\bin\make.exe" wpmcpp.pro -r -spec win32-g++ CONFIG+=release
if %errorlevel% neq 0 exit /b %errorlevel%

"%qt_path%\mingw\bin\mingw32-make.exe"
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..\..

copy LICENSE.txt .Build\32\
if %errorlevel% neq 0 goto out

copy CrystalIcons_LICENSE.txt .Build\32\
if %errorlevel% neq 0 goto out

copy "..\wpmcpp-build-desktop\release\wpmcpp.exe" .Build\32\npackdg.exe
if %errorlevel% neq 0 goto out

rem creating .zip
cd build\32\
"%sevenzipa_path%\7za" a ..\Npackd.zip *
if %errorlevel% neq 0 goto out
cd ..\..

rem creating .msi
"%ai_path%\bin\x86\AdvancedInstaller.com" /build wpmcpp.aip
if %errorlevel% neq 0 goto out


