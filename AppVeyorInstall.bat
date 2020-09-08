echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

msiexec.exe /qn /i https://github.com/tim-lebedkov/npackd-cpp/releases/download/version_1.25/NpackdCL64-1.25.0.msi
if %errorlevel% neq 0 exit /b %errorlevel%

set path=C:\Program Files\NpackdCL;C:\msys64\usr\bin;C:\Windows\System32;C:\Program Files (x86)\7-ZIP

ncl set-repo -u  https://www.npackd.org/rep/recent-xml -u https://www.npackd.org/rep/xml?tag=stable -u https://www.npackd.org/rep/xml?tag=stable64 -u https://www.npackd.org/rep/xml?tag=libs
if %errorlevel% neq 0 exit /b %errorlevel%

ncl detect
if %errorlevel% neq 0 exit /b %errorlevel%

ncl set-install-dir -f "C:\Program Files (x86)"
if %errorlevel% neq 0 exit /b %errorlevel%

ncl add -p com.advancedinstaller.AdvancedInstallerFreeware -r [10,20) -p org.7-zip.SevenZIP -r [9,20) -p exeproxy -r [0.2,1)
if %errorlevel% neq 0 exit /b %errorlevel%

rem Python will be detected, but needs NpackdCL
ncl add -p com.googlecode.windows-package-manager.NpackdCL
if %errorlevel% neq 0 exit /b %errorlevel%

rem update all packages to the newest versions
pacman -Syu --noconfirm
pacman -Syu --noconfirm

pacman -S --noconfirm mingw64/mingw-w64-x86_64-cmake mingw64/mingw-w64-x86_64-ninja mingw64/mingw-w64-x86_64-wget
if %errorlevel% neq 0 exit /b %errorlevel%

if %bits% equ 64 (if %static% equ ON (call :install64static) else (call :install64dynamic)) else (call :install32)

Taskkill /IM gpg-agent.exe /F
Taskkill /IM dirmngr.exe /F
verify
goto :eof

rem ---------------------------------------------------------------------------
:install32
pacman -S --noconfirm mingw-w64-i686-toolchain mingw-w64-i686-libtool mingw32/mingw-w64-i686-jasper mingw32/mingw-w64-i686-qt5-static mingw32/mingw-w64-i686-icu mingw32/mingw-w64-i686-zstd
if %errorlevel% neq 0 exit /b %errorlevel%

ncl add -p quazip-dev-i686-w64_dw2_posix_7.2-qt_5.9.2-static -v 0.7.3 -p drmingw -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b

rem ---------------------------------------------------------------------------
:install64dynamic

pacman -S --noconfirm mingw-w64-x86_64-toolchain mingw-w64-x86_64-libtool mingw64/mingw-w64-x86_64-jasper mingw64/mingw-w64-x86_64-qt5 mingw64/mingw-w64-x86_64-icu mingw64/mingw-w64-x86_64-zstd mingw64/mingw-w64-x86_64-quazip
if %errorlevel% neq 0 exit /b %errorlevel%

ncl add -p drmingw64 -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

exit /b

rem ---------------------------------------------------------------------------
:install64static
pacman -S --noconfirm mingw-w64-x86_64-toolchain mingw-w64-x86_64-libtool mingw64/mingw-w64-x86_64-jasper mingw64/mingw-w64-x86_64-qt5-static mingw64/mingw-w64-x86_64-icu mingw64/mingw-w64-x86_64-zstd
if %errorlevel% neq 0 exit /b %errorlevel%

ncl add -p quazip-dev-x86_64-w64_seh_posix_7.2-qt_5.9.2-static -v 0.7.3 -p drmingw64 -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

if "%prg%" equ "npackd" (call :installcoverity)

exit /b

rem ---------------------------------------------------------------------------
:installcoverity
wget.exe https://scan.coverity.com/download/cxx/win64 --post-data "token=%covtoken%&project=Npackd" -O coverity_tool.zip --no-check-certificate -nv
if %errorlevel% neq 0 exit /b %errorlevel%

7z x -y coverity_tool.zip
if %errorlevel% neq 0 exit /b %errorlevel%

for /f "delims=" %%x in ('dir /b cov-*') do set name=%%x
ren "%name%" cov-analysis


