echo on

rem This script is used by AppVeyor to build the project.

if %bits% equ 64 goto bits64
rem Npackd
set make=C:\Program Files (x86)\MinGW-w64_i686_SJLJ_POSIX_threads\bin\mingw32-make.exe

"%make%" -C wpmcpp zip msi PROFILE=release32 || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\32\release\Npackd32-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\32\release\Npackd32-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\32\release\Npackd32-%version%.map || exit /b %errorlevel%

"%make%" -C npackdcl zip msi PROFILE=release32 || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\32\release\NpackdCL32-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\32\release\NpackdCL32-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact npackdcl\build\32\release\NpackdCL32-%version%.map || exit /b %errorlevel%

goto :eof

:bits64
set make=C:\Program Files (x86)\MinGW-w64_x86_64_SEH_POSIX_threads\bin\mingw32-make.exe
"%make%" -C wpmcpp zip msi PROFILE=release64 || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\64\release\Npackd64-%version%.msi || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\64\release\Npackd64-%version%.zip || exit /b %errorlevel%
appveyor PushArtifact wpmcpp\build\64\release\Npackd64-%version%.map || exit /b %errorlevel%

