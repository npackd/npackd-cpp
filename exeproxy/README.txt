EXE Proxy
=========

This program serves as a proxy for another command line program. All the
arguments passed to this program will also be used for the target program. 
EXE Proxy can be used to enable access to a command line program under another
(possibly shorter) name. The path and file name of the target executable is
stored directly as a resource in a copy of the EXE Proxy. During the start
EXE Proxy reads the resource string and uses it to find the target executable.
The resource string can either contain an absolute file name or a file name
without slashes or backslashes. In the latter case the target executable should
reside in the same directory as the EXE Proxy itself.

In order to change the target executable resource entry you could start the EXE
Proxy with the parameter "exe-proxy-copy":

exe-proxy.exe exe-proxy-copy <output file name> <target executable name>

The second parameter should be the name of the output exe file where a copy of
the EXE Proxy will be stored. The third parameter should either contain an
absolute path to the target executable or the name of the target executable 
without slashes or backslashes if the target executable resides in the same
directory as the EXE Proxy.

