EXE Proxy
=========

This program serves as a proxy for another command line program. All the
arguments will be passed as-is to the target program. The return code of the
target program will be returned as exit code by EXE Proxy.

EXE Proxy can be used to enable access to a command line program under another
(possibly shorter) name. Another usage could be to store many copies of EXE 
proxy in one directory pointing to command line utilities on the computer and
adding only this directory to the PATH environment variable. This would avoid 
reaching the maximum length for PATH (2048 characters) and making it very
complicated while still be able to run all programs without specifying the 
directory where they reside. EXE Proxy could also be useful in making a GUI
executable usable from the command line. Normally cmd.exe does not wait for
GUI executables to end but returns immediately. EXE Proxy does not differentiate
between GUI and non-GUI programs and always waits for the target process to end. 

The path and file name of the target executable are
stored directly as a resource in a copy of the EXE Proxy. During the start
EXE Proxy reads the resource string and uses it to find the target executable.
The resource string can either contain an absolute file name or a file name
without slashes or backslashes. In the latter case the target executable should
reside in the same directory as the EXE Proxy itself.

In order to change the target executable resource entry you could start the EXE
Proxy with the parameter "exe-proxy-copy":

exeproxy.exe exe-proxy-copy <output file name> <target executable name>

The second parameter should be the name of the output exe file where a copy of
the EXE Proxy will be stored. The third parameter should either contain an
absolute path to the target executable or the name of the target executable 
without slashes or backslashes if the target executable resides in the same
directory as the EXE Proxy.

EXE Proxy uses semantic versioning (http://semver.org/). The versions before
1.0 will change the interface incompatibly so please use an exact version 
number as dependency.

