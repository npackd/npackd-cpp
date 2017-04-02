echo on

rem This script is used by AppVeyor to build the project.

where appveyor

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL

if %bits% equ 64 goto bits64

set make=C:\Program Files (x86)\MinGW-w64_i686_SJLJ_POSIX_threads\bin\mingw32-make.exe
goto start

:bits64
set make=C:\Program Files (x86)\MinGW-w64_x86_64_SEH_POSIX_threads\bin\mingw32-make.exe
goto start

:start
if %prg% equ npackdcl goto npackdcl
if %prg% equ clu goto clu

:npackd
if "%target%" equ "coverity" goto coverity

"%make%" -C wpmcpp zip msi zip-debug PROFILE=release%bits%
if %errorlevel% neq 0 exit /b %errorlevel%

tree . /f
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.map
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-debug-%version%.zip
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
"%make%" -C npackdcl zip msi zip-debug PROFILE=release%bits%
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.msi
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.map
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:clu
"%make%" -C clu zip zip-debug PROFILE=release%bits%
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact clu\build\%bits%\release\CLU-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact clu\build\%bits%\release\CLU-%version%.map
if %errorlevel% neq 0 exit /b %errorlevel%

appveyor PushArtifact clu\build\%bits%\release\CLU%bits%-debug-%version%.zip
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

