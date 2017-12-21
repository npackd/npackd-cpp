echo on

rem This script is used by AppVeyor to build the project.

set initial_path=%path%

where appveyor
where cmake

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10,20)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9,20)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set SEVENZIP=%%x

set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=exeproxy" "--versions=[0.2,1)"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set EXEPROXY=%%x

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

mkdir wpmcpp\build
if %errorlevel% neq 0 exit /b %errorlevel%

pushd wpmcpp\build
set path=%mingw%\bin;C:\Program Files (x86)\CMake\bin;%ai%\bin\x86;%sevenzip%
set qtdir=%qt:\=/%
set CMAKE_INCLUDE_PATH=%quazip%\quazip
set CMAKE_LIBRARY_PATH=%quazip%\quazip\release
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=..\install"
if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

pushd ..\install
7z a ..\build\Npackd%bits%-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\src\wpmcpp%bits%.aip .
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\src\app.ico .
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /edit wpmcpp%bits%.aip /SetVersion %version%
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /build wpmcpp%bits%.aip
if %errorlevel% neq 0 exit /b %errorlevel%

popd
popd

set path=%initial_path%

appveyor PushArtifact wpmcpp\build\Npackd%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\install\Npackd%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

rem todo
rem appveyor PushArtifact wpmcpp\build\Npackd%bits%-%version%.map
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem todo
rem appveyor PushArtifact wpmcpp\build\Npackd%bits%-debug-%version%.zip
rem if %errorlevel% neq 0 exit /b %errorlevel%

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
set path=%mingw%\bin;C:\Program Files (x86)\CMake\bin;%ai%\bin\x86;%sevenzip%
set qtdir=%qt:\=/%
set CMAKE_INCLUDE_PATH=%quazip%\quazip
set CMAKE_LIBRARY_PATH=%quazip%\quazip\release
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=..\install"
if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

pushd ..\install

"%EXEPROXY%\exeproxy.exe" exeproxy-copy ncl.exe npackdcl.exe
if %errorlevel% neq 0 exit /b %errorlevel%

7z a ..\build\NpackdCL%bits%-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%
	   
copy ..\src\NpackdCL%bits%.aip .
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\src\app.ico .
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /edit NpackdCL%bits%.aip /SetVersion %version%
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /build NpackdCL%bits%.aip
if %errorlevel% neq 0 exit /b %errorlevel%

popd
popd

set path=%initial_path%

appveyor PushArtifact npackdcl\build\NpackdCL%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\install\NpackdCL%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

rem todo
rem appveyor PushArtifact npackdcl\build\NpackdCL%bits%-%version%.map
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem todo
rem appveyor PushArtifact npackdcl\build\NpackdCL%bits%-debug-%version%.zip
rem if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:clu
mkdir clu\build
if %errorlevel% neq 0 exit /b %errorlevel%

pushd clu\build
set path=%mingw%\bin;C:\Program Files (x86)\CMake\bin;%ai%\bin\x86;%sevenzip%
set qtdir=%qt:\=/%
set CMAKE_INCLUDE_PATH=%quazip%\quazip
set CMAKE_LIBRARY_PATH=%quazip%\quazip\release
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=..\install"
if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

pushd ..\install

7z a ..\build\CLU%bits%-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%
	   
copy ..\src\app.ico .
if %errorlevel% neq 0 exit /b %errorlevel%

popd
popd

set path=%initial_path%

appveyor PushArtifact clu\build\CLU%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

