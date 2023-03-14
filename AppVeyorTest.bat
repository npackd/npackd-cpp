echo on

rem This script is used by AppVeyor to test the project.

if %bits% equ 64 goto :eof
if %prg% neq npackdcl goto :eof

set path=C:\msys64\mingw32\bin;%ai%\bin\x86;%sevenzip%

cd npackdcl\install

tests -v2
if %errorlevel% neq 0 exit /b %errorlevel%

rem 40 minutes timeout for *all* tests
set QTEST_FUNCTION_TIMEOUT=2400000
ftests -v2
if %errorlevel% neq 0 exit /b %errorlevel%
