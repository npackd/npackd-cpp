echo on

rem This script is used by AppVeyor automatic builds to install the necessary
rem software dependencies.

echo %NUMBER_OF_PROCESSORS%

msiexec.exe /qn /i https://github.com/tim-lebedkov/npackd-cpp/releases/download/version_1.25/NpackdCL64-1.25.0.msi
if %errorlevel% neq 0 exit /b %errorlevel%

SET NPACKD_CL=C:\Program Files\NpackdCL
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

"%npackd_cl%\ncl" list --json

rem update the keyring according to https://www.msys2.org/news/#2020-06-29-new-packagers
curl -O http://repo.msys2.org/msys/x86_64/msys2-keyring-r21.b39fb11-1-any.pkg.tar.xz
curl -O http://repo.msys2.org/msys/x86_64/msys2-keyring-r21.b39fb11-1-any.pkg.tar.xz.sig
rem pacman-key is probably not installed
rem C:\msys64\usr\bin\pacman-key --verify msys2-keyring-r21.b39fb11-1-any.pkg.tar.xz.sig
C:\msys64\usr\bin\pacman -U --noconfirm msys2-keyring-r21.b39fb11-1-any.pkg.tar.xz
C:\msys64\usr\bin\pacman -U --noconfirm --config <(echo) msys2-keyring-r21.b39fb11-1-any.pkg.tar.xz

rem update all packages to the newest versions
C:\msys64\usr\bin\pacman -Syu --noconfirm 
C:\msys64\usr\bin\pacman -Syu --noconfirm 

if %bits% equ 64 goto bits64

C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-i686-libtool mingw32/mingw-w64-i686-jasper mingw32/mingw-w64-i686-qt5-static mingw32/mingw-w64-i686-icu mingw32/mingw-w64-i686-zstd
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" add -p quazip-dev-i686-w64_dw2_posix_7.2-qt_5.9.2-static -v 0.7.3 -p drmingw -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

goto :eof

:bits64

if %static% equ ON goto staticbits64

C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-x86_64-libtool mingw64/mingw-w64-x86_64-jasper mingw64/mingw-w64-x86_64-qt5 mingw64/mingw-w64-x86_64-icu mingw64/mingw-w64-x86_64-zstd mingw64/mingw-w64-x86_64-quazip
if %errorlevel% neq 0 exit /b %errorlevel%

"%npackd_cl%\ncl" add -p drmingw64 -v 0.7.7
if %errorlevel% neq 0 exit /b %errorlevel%

goto end

:staticbits64
C:\msys64\usr\bin\pacman -S --noconfirm mingw-w64-x86_64-libtool mingw64/mingw-w64-x86_64-jasper mingw64/mingw-w64-x86_64-qt5-static mingw64/mingw-w64-x86_64-icu mingw64/mingw-w64-x86_64-zstd
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

