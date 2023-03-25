echo on
rem This script is used by AppVeyor to build the project.
set initial_path=%path%
set version=%APPVEYOR_BUILD_VERSION:~0,-5%
SET NPACKD_CL=C:\Program Files\NpackdCL

set onecmd="%npackd_cl%\ncl.exe" path -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,100)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set ai=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p org.7-zip.SevenZIP -r [9,100)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set SEVENZIP=%%x

set onecmd="%npackd_cl%\ncl.exe" path -p exeproxy -r [0.2,1)
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set EXEPROXY=%%x


if %bits% equ 64 (call :setvars64) else (call :setvars32)
set path=%mingw%\bin;C:\msys64\mingw64\bin\

mkdir c:\builds

call :buildquazip
if %prg% equ npackdcl (call :buildnpackdcl)
if %prg% equ clu (call :buildclu)
if %prg% equ npackd (call :buildnpackd)

goto :eof

rem ---------------------------------------------------------------------------
:setvars32
set mingw_libs=i686-w64-mingw32
set mingw=C:\msys64\mingw32
set quazip=%cd%\build-quazip
set onecmd="%npackd_cl%\ncl.exe" path -p drmingw -v 0.7.7
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x
exit /b

rem ---------------------------------------------------------------------------
:setvars64
set mingw_libs=x86_64-w64-mingw32
set mingw=C:\msys64\mingw64

set onecmd="%npackd_cl%\ncl.exe" path -p drmingw64 -v 0.7.7
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set drmingw=%%x
exit /b

rem ---------------------------------------------------------------------------
:buildquazip
cmake.exe -GNinja -DCMAKE_BUILD_TYPE=Release -DQUAZIP_QT_MAJOR_VERSION=5 -DQUAZIP_FETCH_LIBS=OFF -S quazip -B c:\builds\quazip -DBUILD_SHARED_LIBS:BOOL=OFF -DZLIB_LIBRARY_RELEASE=%mingw%\lib\libz.a -DZLIB_LIBRARY_RELEASE=%mingw%\lib\libbz2.a
if %errorlevel% neq 0 exit /b %errorlevel%

cmake.exe --build c:\builds\quazip
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b

rem ---------------------------------------------------------------------------
:buildnpackd
set path=%mingw%\bin;%ai%\bin\x86;%sevenzip%;C:\msys64\mingw64\bin
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%;%quazip%\quazip

cmake -GNinja -S npackdg -B c:\builds\npackdg -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=..\install -DNPACKD_FORCE_STATIC:BOOL=%STATIC% -DQUAZIP_INCLUDE_DIRS=quazip -DQUAZIP_LIBRARIES=c:\builds\quazip
if %errorlevel% neq 0 exit /b %errorlevel%

cmake.exe --build c:\builds\npackdg
if %errorlevel% neq 0 exit /b %errorlevel%

C:\Windows\System32\xcopy.exe ..\install ..\install-debug /E /I /H /Y
if %errorlevel% neq 0 exit /b %errorlevel%

strip ..\install\npackdg.exe
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\build\npackdg.map .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\exchndl.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\mgwhelp.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\dbghelp.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\symsrv.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\symsrv.yes" .
if %errorlevel% neq 0 exit /b %errorlevel%

7z a ..\build\Npackd%bits%-debug-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%

7z a ..\build\Npackd%bits%-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\src\npackdg%bits%.aip .
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\src\app.ico .
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /edit npackdg%bits%.aip /SetVersion %version%
if %errorlevel% neq 0 exit /b %errorlevel%

AdvancedInstaller.com /build npackdg%bits%.aip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdg\build\Npackd%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdg\install\Npackd%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdg\build\Npackd%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b

rem ---------------------------------------------------------------------------
:coverity
rem Coverity build is too slow

set path=%mingw%\bin;%ai%\bin\x86;%sevenzip%;C:\msys64\mingw64\bin
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%;%quazip%\quazip
mingw32-make.exe clean

"..\..\cov-analysis\bin\cov-configure.exe"  --comptype gcc --compiler C:\PROGRA~2\MINGW-~1\bin\G__~1.EXE -- -std=gnu++11
"..\..\cov-analysis\bin\cov-build.exe" --dir ..\..\cov-int mingw32-make.exe install

rem type C:\projects\Npackd\cov-int\build-log.txt

7z a cov-int.zip cov-int
if %errorlevel% neq 0 exit /b %errorlevel%

"C:\Program Files (x86)\Gow\bin\curl" --form token=%covtoken% --form email=tim.lebedkov@gmail.com -k --form file=@cov-int.zip --form version="Version" --form description="Description" https://scan.coverity.com/builds?project=Npackd
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b

rem ---------------------------------------------------------------------------
:buildnpackdcl
set path=%mingw%\bin;%ai%\bin\x86;%sevenzip%;C:\msys64\mingw64\bin
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=..\install -DNPACKD_FORCE_STATIC:BOOL=%STATIC% -DQUAZIP_INCLUDE_DIRS=quazip -DQUAZIP_LIBRARIES=c:\builds\quazip
rem C:\Windows\System32\notepad.exe

if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

"%EXEPROXY%\exeproxy.exe" exeproxy-copy ..\install\ncl.exe npackdcl.exe
if %errorlevel% neq 0 exit /b %errorlevel%

del ..\install\tests.exe
if %errorlevel% neq 0 exit /b %errorlevel%

del ..\install\ftests.exe
if %errorlevel% neq 0 exit /b %errorlevel%

C:\Windows\System32\xcopy.exe ..\install ..\install-debug /E /I /H /Y
if %errorlevel% neq 0 exit /b %errorlevel%

strip ..\install\npackdcl.exe
if %errorlevel% neq 0 exit /b %errorlevel%

copy ..\build\npackdcl.map .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\exchndl.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\mgwhelp.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\dbghelp.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\symsrv.dll" .
if %errorlevel% neq 0 exit /b %errorlevel%

copy "%DRMINGW%\bin\symsrv.yes" .
if %errorlevel% neq 0 exit /b %errorlevel%

7z a ..\build\NpackdCL%bits%-debug-%version%.zip * -mx9	
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

set path=%initial_path%

appveyor PushArtifact npackdcl\build\NpackdCL%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\install\NpackdCL%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\NpackdCL%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b

rem ---------------------------------------------------------------------------
:buildnpackdcl
set path=%mingw%\bin;%ai%\bin\x86;%sevenzip%;C:\msys64\mingw64\bin
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%

cmake ..\ -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=..\install -DNPACKD_FORCE_STATIC:BOOL=%STATIC%
if %errorlevel% neq 0 exit /b %errorlevel%

mingw32-make.exe install
if %errorlevel% neq 0 exit /b %errorlevel%

strip ..\install\clu.exe
if %errorlevel% neq 0 exit /b %errorlevel%

7z a ..\build\CLU%bits%-%version%.zip * -mx9	
if %errorlevel% neq 0 exit /b %errorlevel%
	   
copy ..\src\app.ico .
if %errorlevel% neq 0 exit /b %errorlevel%

set path=%initial_path%

appveyor PushArtifact clu\build\CLU%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b