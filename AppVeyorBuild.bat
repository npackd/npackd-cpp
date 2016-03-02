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

:npackd
if "%target%" equ "coverity" goto coverity
"%make%" -C wpmcpp zip msi zip-debug PROFILE=release%bits% || exit /b %errorlevel%
tree . /f
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.map || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-debug-%version%.zip || exit /b %errorlevel%
goto :eof

:coverity
"cov-analysis\bin\cov-build.exe" --dir cov-int "%make%" -C wpmcpp compile PROFILE=release%bits% || exit /b %errorlevel%
7z a cov-int.zip cov-int || exit /b %errorlevel%
curl --form token=%covtoken% --form email=tim.lebedkov@gmail.com --form file=@cov-int.zip --form version="Version" --form description="Description" https://scan.coverity.com/builds?project=Npackd || exit /b %errorlevel%
goto :eof

:npackdcl
"%make%" -C npackdcl zip msi zip-debug PROFILE=release%bits% || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.map || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL%bits%-debug-%version%.zip || exit /b %errorlevel%
goto :eof

