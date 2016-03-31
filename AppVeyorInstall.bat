echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

msiexec.exe /qn /i https://github.com/tim-lebedkov/npackd-cpp/releases/download/version_1.21.5/NpackdCL-1.21.5.msi || exit /b %errorlevel%
SET NPACKD_CL=C:\Program Files (x86)\NpackdCL|| exit /b %errorlevel%
"%npackd_cl%\ncl" add-repo --url=https://npackd.appspot.com/rep/recent-xml || exit /b %errorlevel%
"%npackd_cl%\ncl" add-repo --url=https://npackd.appspot.com/rep/xml?tag=libs || exit /b %errorlevel%
"%npackd_cl%\ncl" detect || exit /b %errorlevel%
"%npackd_cl%\ncl" set-install-dir -f "C:\Program Files (x86)" || exit /b %errorlevel%

if "%target%" equ "drmemory" (
    "%npackd_cl%\ncl" add -p drmemory || exit /b %errorlevel%
)

if %bits% equ 64 goto bits64
"%npackd_cl%\ncl" add -p npackd-dev-i686-w64 -v %version% || exit /b %errorlevel%
goto :eof

:bits64
"%npackd_cl%\ncl" add -p npackd-dev-x86_64-w64 -v %version% || exit /b %errorlevel%

if "%target%" neq "coverity" goto end
"%npackd_cl%\ncl" add -p com.github.bmatzelle.Gow -v 0.8
dir "C:\Program Files (x86)"
"C:\Program Files (x86)\Gow\bin\wget" https://scan.coverity.com/download/cxx/win_64 --post-data "token=%covtoken%&project=Npackd" -O coverity_tool.zip --no-check-certificate -nv
7z x -y coverity_tool.zip
for /f "delims=" %%x in ('dir /b cov-*') do set name=%%x
ren "%name%" cov-analysis

:end
rem tree "C:\Program Files (x86)"

