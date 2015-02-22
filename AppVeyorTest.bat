rem This script is used by AppVeyor to build the project.
rem
rem Parameters:
rem %1 - type of the target system: 32 or 64

if %bits% equ 64 goto bits64
set make=C:\Program Files (x86)\MinGW-w64_i686_SJLJ_POSIX_threads\bin\mingw32-make.exe
"%make%" -C npackdcl\tests compile PROFILE=release32
npackdcl\tests\build\32\release\tests -v2
"%make%" -C npackdcl\ftests compile PROFILE=release32
npackdcl\ftests\build\32\release\ftests -v2
goto :eof

:bits64

