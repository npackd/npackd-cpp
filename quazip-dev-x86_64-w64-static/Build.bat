set onecmd="%npackd_cl%\npackdcl.exe" path "--package=net.sourceforge.mingw-w64.MinGWW6464" "--versions=[4.7.2, 4.7.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingww64=%%x

set onecmd="%npackd_cl%\npackdcl.exe" path "--package=com.nokia.QtDev-x86_64-w64-Npackd-Release" "--versions=[4.8.2, 4.8.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set qtsource=%%x

set path=%mingww64%\bin

"%qtsource%\qmake\qmake.exe" CONFIG+=staticlib CONFIG+=release INCLUDEPATH+="%qtsource:\=\\%\\src\\3rdparty\\zlib" 
if %errorlevel% neq 0 exit /b %errorlevel%
                    
"%mingww64%\bin\mingw32-make.exe"
if %errorlevel% neq 0 exit /b %errorlevel%

