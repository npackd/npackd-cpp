echo on

rem This script is used by AppVeyor to build the project.

where appveyor

if %bits% equ 64 goto bits64

set make=C:\Program Files (x86)\MinGW-w64_i686_SJLJ_POSIX_threads\bin\mingw32-make.exe
goto start

:bits64
set make=C:\Program Files (x86)\MinGW-w64_x86_64_SEH_POSIX_threads\bin\mingw32-make.exe
goto start

:start
if %prg% equ npackdcl goto npackdcl

:npackd
"%make%" -C wpmcpp zip msi PROFILE=release%bits% || exit /b %errorlevel%
tree . /f
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\%bits%\release\Npackd%bits%-%version%.map || exit /b %errorlevel%
goto :eof

:npackdcl
"%make%" -C npackdcl zip msi PROFILE=release%bits% || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\%bits%\release\NpackdCL-%version%.map || exit /b %errorlevel%
goto :eof

