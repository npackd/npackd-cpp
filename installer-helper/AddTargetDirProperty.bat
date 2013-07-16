rem since 1.2
rem This script adds the property named TARGETDIR to an .msi file
rem 
rem Script parameters: none
for /f "delims=" %%x in ('dir /b *.msi') do set setup=%%x
"%SYSTEMROOT%\System32\cscript.exe" "%~dp0\AddTargetDirProperty.vbs" //B //NoLogo //U //E:VBScript "%setup%"

