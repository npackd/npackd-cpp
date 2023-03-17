set version=%APPVEYOR_BUILD_VERSION:~0,-4%
set mingw_libs=i686-w64-mingw32
set mingw=C:\msys64\mingw32
SET NPACKD_CL=C:\Program Files\NpackdCL
set path=%mingw%\bin;%ai%\bin\x86;%sevenzip%

set onecmd="%npackd_cl%\ncl.exe" path -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,100)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p org.7-zip.SevenZIP -r [9,100)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set SEVENZIP=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p exeproxy -r [0.2,1)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set EXEPROXY=%%x


set onecmd="%npackd_cl%\ncl.exe" path -p quazip-dev-i686-w64_dw2_posix_7.2-qt_5.9.2-static -v 0.7.3
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p drmingw -v 0.7.7
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x

rem set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%;%quazip%

rem TODO  -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=..\install -DNPACKD_FORCE_STATIC:BOOL=%STATIC%
meson setup build\meson
if %errorlevel% neq 0 exit /b %errorlevel%

meson compile -C build\meson
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir build\dist

copy build\meson\npackdg.exe build\dist
if %errorlevel% neq 0 exit /b %errorlevel%

copy src\CrystalIcons_LICENSE.txt build\dist
if %errorlevel% neq 0 exit /b %errorlevel%

copy src\LICENSE.txt build\dist
if %errorlevel% neq 0 exit /b %errorlevel%

rem TODO exeproxy

C:\Windows\System32\xcopy.exe build\dist build\dist-debug /E /I /H /Y
if %errorlevel% neq 0 exit /b %errorlevel%

strip dist\npackdg.exe
if %errorlevel% neq 0 exit /b %errorlevel%

copy build\meson\npackdg.map build\dist-debug
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\exchndl.dll" build\dist-debug
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\mgwhelp.dll" build\dist-debug
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\dbghelp.dll" build\dist-debug
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\symsrv.dll" build\dist-debug
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\symsrv.yes" build\dist-debug
if %errorlevel% neq 0 exit /b %errorlevel%

7z a build\Npackd%bits%-debug-%version%.zip build\dist-debug\* -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%

7z a build\Npackd%bits%-%version%.zip build\dist\* -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%

copy src\npackdg%bits%.aip build\dist
if %errorlevel% neq 0 exit /b %errorlevel%

copy src\app.ico build\dist
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /edit build\dist\npackdg%bits%.aip /SetVersion %version%
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /build build\dist\npackdg%bits%.aip
if %errorlevel% neq 0 exit /b %errorlevel%

