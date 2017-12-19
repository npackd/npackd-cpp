echo on

rem This script is used by AppVeyor to build the project.

set initial_path=%path%

where appveyor
where cmake

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL

if %bits% equ 64 goto bits64

set QT=C:\NpackdSymlinks\com.nokia.QtDev-i686-w64-Npackd-Release-5.5
set PACKAGE=com.googlecode.windows-package-manager.Npackd
set mingw_libs=i686-w64-mingw32

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=mingw-w64-i686-sjlj-posix" "--versions=[4.9.2,4.9.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=quazip-dev-i686-w64-static" "--versions=[0.7.1,0.7.1]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=drmingw" "--versions=[0.7.7,0.7.7]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x

goto start

:bits64

set QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.5
set PACKAGE=com.googlecode.windows-package-manager.Npackd64
set mingw_libs=x86_64-w64-mingw32

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.9.2,4.9.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.7.1,0.7.1]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set quazip=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=drmingw64" "--versions=[0.7.7,0.7.7]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x

goto start

:start
if %prg% equ npackdcl goto npackdcl
if %prg% equ clu goto clu

:npackd
if "%target%" equ "coverity" goto coverity

"%make%" -C wpmcpp zip msi zip-debug PROFILE=release%bits%
if %errorlevel% neq 0 exit /b %errorlevel%

tree . /f
appveyor PushArtifact wpmcpp\build\Npackd%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\build\Npackd%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\build\Npackd%bits%-%version%.map
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\build\Npackd%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:coverity
"cov-analysis\bin\cov-build.exe" --dir cov-int "%make%" -C wpmcpp compile PROFILE=release%bits% || exit /b %errorlevel%
7z a cov-int.zip cov-int
if %errorlevel% neq 0 exit /b %errorlevel%

"C:\Program Files (x86)\Gow\bin\curl" --form token=%covtoken% --form email=tim.lebedkov@gmail.com --form file=@cov-int.zip --form version="Version" --form description="Description" https://scan.coverity.com/builds?project=Npackd
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:npackdcl
mkdir npackdcl\build
if %errorlevel% neq 0 exit /b %errorlevel%

pushd npackdcl\build
set path=%mingw%\bin;C:\Program Files (x86)\CMake\bin
set qtdir=%qt:\=/%
set CMAKE_INCLUDE_PATH=%quazip%\quazip
set CMAKE_LIBRARY_PATH=%quazip%\quazip\release
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=%cd%\npackdcl\install"
if %errorlevel% neq 0 exit /b %errorlevel%

rem todo -C npackdcl zip msi zip-debug PROFILE=release%bits%
mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

popd

set path=%initial_path%

appveyor PushArtifact npackdcl\build\NpackdCL-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\NpackdCL-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\NpackdCL-%version%.map
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\NpackdCL%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:clu
"%make%" -C clu zip zip-debug PROFILE=release%bits%
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact clu\build\CLU-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact clu\build\CLU-%version%.map
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact clu\build\CLU%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

