
echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

msiexec.exe /qn /i https://github.com/tim-lebedkov/npackd-cpp/releases/download/version_1.23.2/NpackdCL-1.23.2.msi
if %errorlevel% neq 0 exit /b %errorlevel%

SET NPACKD_CL=C:\Program Files (x86)\NpackdCL
"%npackd_cl%\ncl" set-repo -u  https://www.npackd.org/rep/recent-xml -u https://www.npackd.org/rep/xml?tag=stable -u https://www.npackd.org/rep/xml?tag=stable64 -u https://www.npackd.org/rep/xml?tag=libs
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" detect
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" set-install-dir -f "C:\Program Files (x86)"
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" add -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,20) -p org.7-zip.SevenZIP -r [9,20) -p exeproxy -r [0.2,1)
if %errorlevel% neq 0 exit /b %errorlevel%

rem Python will be detected, but needs NpackdCL
"%npackd_cl%\ncl" add -p com.googlecode.windows-package-manager.NpackdCL
if %errorlevel% neq 0 exit /b %errorlevel%

rem update all packages to the newest versions
C:\msys64\usr\bin\pacman -Syu --noconfirm 

if %bits% equ 64 goto bits64

C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-i686-libtool mingw32/mingw-w64-i686-jasper mingw32/mingw-w64-i686-qt5-static mingw32/mingw-w64-i686-icu
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" add -p quazip-dev-i686-w64_dw2_posix_7.2-qt_5.9.2-static -v 0.7.3 -p drmingw -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:bits64
C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-x86_64-libtool  mingw64/mingw-w64-x86_64-jasper mingw64/mingw-w64-x86_64-qt5-static mingw64/mingw-w64-x86_64-icu
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" add -p quazip-dev-x86_64-w64_seh_posix_7.2-qt_5.9.2-static -v 0.7.3 -p drmingw64 -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

if "%prg%" neq "npackd" goto end

"C:\msys64\usr\bin\wget.exe" https://scan.coverity.com/download/cxx/win64 --post-data "token=%covtoken%&project=Npackd" -O coverity_tool.zip --no-check-certificate -nv
if %errorlevel% neq 0 exit /b %errorlevel%

7z x -y coverity_tool.zip
if %errorlevel% neq 0 exit /b %errorlevel%

for /f "delims=" %%x in ('dir /b cov-*') do set name=%%x
ren "%name%" cov-analysis

:end
rem tree "C:\Program Files (x86)"

