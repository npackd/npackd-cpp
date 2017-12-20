echo on

rem This script is used by AppVeyor to build the project.

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL

if %bits% equ 64 goto bits64

set make=C:\Program Files (x86)\MinGW-w64_i686_SJLJ_POSIX_threads\bin\mingw32-make.exe
goto start

:bits64
set make=C:\Program Files (x86)\MinGW-w64_x86_64_SEH_POSIX_threads\bin\mingw32-make.exe
goto start

:start
if %prg% equ npackdcl goto npackdcl
goto :eof

:npackdcl
rem todo
rem "%make%" -C npackdcl\tests compile PROFILE=release%bits%
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem npackdcl\tests\build\%bits%\release\tests -v2
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem "%make%" -C npackdcl\ftests compile PROFILE=release%bits%
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem npackdcl\ftests\build\%bits%\release\ftests -v2
rem if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof


