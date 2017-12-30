
echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

msiexec.exe /qn /i https://github.com/tim-lebedkov/npackd-cpp/releases/download/version_1.23.2/NpackdCL-1.23.2.msi
if %errorlevel% neq 0 exit /b %errorlevel%

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL
"%npackd_cl%\ncl" set-repo -u https://www.npackd.org/rep/xml?tag=stable -u https://www.npackd.org/rep/xml?tag=stable64 -u  https://www.npackd.org/rep/recent-xml -u https://www.npackd.org/rep/xml?tag=libs
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" detect
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" set-install-dir -f "C:\Program Files (x86)"
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" add -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,20) -p org.7-zip.SevenZIP -r [9,20) -p exeproxy -r [0.2,1)
if %errorlevel% neq 0 exit /b %errorlevel%

if %bits% equ 64 goto bits64

"%npackd_cl%\ncl" add -p mingw-w64-i686-sjlj-posix -v 4.9.2 -p com.nokia.QtDev-i686-w64-Npackd-Release -v 5.5 -p quazip-dev-i686-w64-static -v 0.7.1 -p drmingw -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:bits64
"%npackd_cl%\ncl" add -p mingw-w64-x86_64-seh-posix -v 4.9.2 -p com.nokia.QtDev-x86_64-w64-Npackd-Release -v 5.5 -p quazip-dev-x86_64-w64-static -v 0.7.1 -p drmingw64 -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

if "%prg%" neq "npackd" goto end

"%npackd_cl%\ncl" add -p com.github.bmatzelle.Gow -v 0.8
if %errorlevel% neq 0 exit /b %errorlevel%

dir "C:\Program Files (x86)"
"C:\Program Files (x86)\Gow\bin\wget" https://scan.coverity.com/download/cxx/win64 --post-data "token=%covtoken%&project=Npackd" -O coverity_tool.zip --no-check-certificate -nv
if %errorlevel% neq 0 exit /b %errorlevel%

7z x -y coverity_tool.zip
if %errorlevel% neq 0 exit /b %errorlevel%

for /f "delims=" %%x in ('dir /b cov-*') do set name=%%x
ren "%name%" cov-analysis

:end
rem tree "C:\Program Files (x86)"

