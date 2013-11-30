set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW64" "--versions=[4.7.2, 4.7.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

%mingw%\bin\gprof.exe --help > GProf32.txt

rem use --no-demangle to find out a mangled function name
rem  -f _ZN12DBRepository8updateF5EP3Job
rem -qDBRepository::updateF5(Job*) 
%mingw%\bin\gprof.exe -a -F _ZN12DBRepository8updateF5EP3Job ..\wpmcpp-i686-gprof\release\wpmcpp.exe ..\wpmcpp-i686-gprof\release\gmon.out >> GProf32.txt

