rem This script is used by AppVeyor to build the project.
rem
rem Parameters:
rem %1 - type of the target system: 32 or 64

if %bits% equ 64 goto bits64
rem Npackd
set make=C:\Program Files (x86)\MinGW-w64_i686_SJLJ_POSIX_threads\bin\mingw32-make.exe
"%make%" -C wpmcpp zip msi PROFILE=release32
"%make%" -C npackdcl zip msi PROFILE=release32
goto :eof

:bits64
set make=C:\Program Files (x86)\MinGW-w64_x86_64_SEH_POSIX_threads\bin\mingw32-make.exe
"%make%" -C wpmcpp zip msi PROFILE=release64

