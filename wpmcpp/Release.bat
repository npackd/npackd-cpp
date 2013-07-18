rem set mingw_path=C:/ProgramFiles/MinGW-w64
rem set qt_path=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-4.8.2
rem set ai_path=C:/Program Files (x86)/Advanced_Installer_Freeware
rem set quazip_path=C:/Users/t/projects/library-builder/QuaZIP-static-i686-w32-0.5
rem set zlib_path=C:/Users/t/projects/library-builder/zlib-i686-w32-1.2.5
rem set sevenzipa_path=C:/Program Files (x86)/7-ZIP_A
rem set msi_path=C:/Users/t/projects/library-builder/MSI-i686-w32-5
rem set gdb_path=C:\Program Files\GDB_64_bit
rem
call Paths.bat
if %errorlevel% neq 0 exit /b %errorlevel%

rmdir /S /Q build

mkdir build
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir build\release32
if %errorlevel% neq 0 exit /b %errorlevel%

set path=%qt_path%\mingw\bin
cd build\release32
"%qt_path%\Desktop\Qt\4.7.3\mingw\bin\make.exe" wpmcpp.pro -r -spec win32-g++ CONFIG+=release
if %errorlevel% neq 0 exit /b %errorlevel%

"%qt_path%\mingw\bin\mingw32-make.exe"
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..\..

copy LICENSE.txt .Build\32\
if %errorlevel% neq 0 goto out

copy CrystalIcons_LICENSE.txt .Build\32\
if %errorlevel% neq 0 goto out

copy "..\wpmcpp-build-desktop\release\wpmcpp.exe" .Build\32\npackdg.exe
if %errorlevel% neq 0 goto out

rem creating .zip
cd build\32\
"%sevenzipa_path%\7za" a ..\Npackd.zip *
if %errorlevel% neq 0 goto out
cd ..\..

rem creating .msi
"%ai_path%\bin\x86\AdvancedInstaller.com" /build wpmcpp.aip
if %errorlevel% neq 0 goto out

