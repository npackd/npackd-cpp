echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

msiexec.exe /qn /i http://bit.ly/npackdcl-1_19_13 || exit /b %errorlevel%
SET NPACKD_CL=C:\Program Files (x86)\NpackdCL|| exit /b %errorlevel%
"%npackd_cl%\ncl" add-repo --url=https://npackd.appspot.com/rep/recent-xml || exit /b %errorlevel%
"%npackd_cl%\ncl" add-repo --url=https://npackd.appspot.com/rep/xml?tag=libs || exit /b %errorlevel%
"%npackd_cl%\ncl" detect || exit /b %errorlevel%
"%npackd_cl%\ncl" set-install-dir -f "C:\Program Files (x86)" || exit /b %errorlevel%

if %bits% equ 64 goto bits64
"%npackd_cl%\ncl" add -p npackd-dev-i686-w64 -v %version% || exit /b %errorlevel%
goto :eof

:bits64
"%npackd_cl%\ncl" add -p npackd-dev-x86_64-w64 -v %version% || exit /b %errorlevel%
if "%coverity%" equ "yes" goto coverity
goto :eof

:coverity
wget https://scan.coverity.com/download/cxx/win_64 --post-data "token=%covtoken%&project=Npackd" -O coverity_tool.zip
7z e -y coverity_tool.zip
goto :eof

