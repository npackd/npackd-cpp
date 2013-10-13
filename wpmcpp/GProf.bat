set onecmd="%npackd_cl%\npackdcl.exe" "path" "--package=net.sourceforge.mingw-w64.MinGWW6464" "--versions=[4.7.2, 4.7.2]"
for /f "usebackq delims=" %%x in (`%%onecmd%%`) do set mingw=%%x

%mingw%\bin\gprof.exe --help > GProf.txt

rem use --no-demangle to find out a mangled function name
rem  -f _ZN12DBRepository8updateF5EP3Job
%mingw%\bin\gprof.exe -qDBRepository::updateF5(Job*) ..\wpmcpp-x86_64-gprof\release\wpmcpp.exe ..\wpmcpp-x86_64-gprof\gmon.out >> GProf.txt

