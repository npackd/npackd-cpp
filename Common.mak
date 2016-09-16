ifeq (32,$(findstring 32,$(PROFILE)))
QT=C:\NpackdSymlinks\com.nokia.QtDev-i686-w64-Npackd-Release-5.5
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-i686-sjlj-posix" "--versions=[4.9.2, 4.9.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-i686-w64-static" "--versions=[0.7.1, 0.7.1]")
DRMINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=drmingw" "--versions=[0.7.7, 0.7.7]")
BITS=32
PACKAGE=com.googlecode.windows-package-manager.CLU
else
QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.5
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.9.2, 4.9.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.7.1, 0.7.1]")
DRMINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=drmingw64" "--versions=[0.7.7, 0.7.7]")
BITS=64
endif
