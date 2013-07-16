call Paths.bat
if %errorlevel% neq 0 exit /b %errorlevel%

rmdir /S /Q build

mkdir build
if %errorlevel% neq 0 exit /b %errorlevel%

rem copying files
rem copy "%mingw_path%\bin\libgcc_s_dw2-1.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy "%mingw_path%\bin\mingwm10.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy LICENSE.txt .Build\32\
rem if %errorlevel% neq 0 goto out

rem copy CrystalIcons_LICENSE.txt .Build\32\
rem if %errorlevel% neq 0 goto out

rem copy "..\wpmcpp-build-desktop\release\wpmcpp.exe" .Build\32\npackdg.exe
rem if %errorlevel% neq 0 goto out

rem copy "%qt_path%\bin\QtCore4.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy "%qt_path%\bin\QtGui4.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy "%qt_path%\bin\QtNetwork4.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy "%qt_path%\bin\QtXml4.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy "%quazip_path%\quazip\release\quazip.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem copy "%zlib_path%\zlib1.dll" .Build\32\
rem if %errorlevel% neq 0 exit /b %errorlevel%

rem creating .zip
rem cd .Build\32\
rem "%sevenzipa_path%\7za" a ..\Npackd.zip *
rem if %errorlevel% neq 0 goto out
rem cd ..\..

rem creating .msi
rem "%ai_path%\bin\x86\AdvancedInstaller.com" /build wpmcpp.aip
rem if %errorlevel% neq 0 goto out

