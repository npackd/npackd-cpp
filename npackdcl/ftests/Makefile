# This is the main build file for ftests

# Parameter: release64
PROFILE=release64

# ------------------------------------------------------------------------------

.PHONY: all printvars clean compile prep zip msi gprof drmemory upload

PROJECT=ftests

QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.2.1
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.8.2, 4.8.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.6.2, 0.6.2]")
BITS=64

CONFIG=release
WHERE=build\64\release

RPAREN=)
AI=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 20$(RPAREN)")
SEVENZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 10$(RPAREN)")
MINGWUTILS=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.mingw.MinGWUtilities" "--versions=[0.3, 0.3]")
PUTTY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=uk.org.greenend.chiark.sgtatham.Putty" "--versions=[0.62, 2$(RPAREN)")
EXEPROXY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=exeproxy" "--versions=[0.1, 0.1]")

all:
	$(MAKE) clean PROFILE=release64
	$(MAKE) compile PROFILE=release64
	@echo ======================= SUCCESS =======================================

printvars:
	@echo PROFILE=$(PROFILE)
	@echo BITS=$(BITS)
	@echo MINGW=$(MINGW)
	@echo QUAZIP=$(QUAZIP)
	@echo QT=$(QT)
	@echo AI=$(AI)
	@echo SEVENZIP=$(SEVENZIP)
	@echo QUAZIP=$(QUAZIP)
	@echo MINGWUTILS=$(MINGWUTILS)
	@echo PUTTY=$(PUTTY)
	@echo WHERE=$(WHERE)
	@echo CONFIG=$(CONFIG)
ifndef PROFILE
	$(error PROFILE is not defined)
endif
ifndef BITS
	$(error BITS is not defined)
endif
ifndef QT
	$(error QT is not defined)
endif
ifndef MINGW
	$(error MINGW is not defined)
endif
ifndef AI
	$(error AI is not defined)
endif
ifndef SEVENZIP
	$(error SEVENZIP is not defined)
endif
ifndef QUAZIP
	$(error QUAZIP is not defined)
endif
ifndef PUTTY
	$(error PUTTY is not defined)
endif
ifndef MINGWUTILS
	$(error MINGWUTILS is not defined)
endif

clean: printvars
	-rmdir /s /q $(WHERE)

$(WHERE)/zip:
	-mkdir $(WHERE)\zip

$(WHERE):
	-mkdir $(WHERE)

$(WHERE)/../Makefile: src/$(PROJECT).pro $(WHERE) 
	rem note how && directly follows \bin. Otherwise the path would contain a space
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(QT)\qtbase\bin\qmake.exe" ..\..\src\$(PROJECT).pro -r -spec win32-g++ CONFIG+=$(CONFIG)

compile: printvars $(WHERE) $(WHERE)/../Makefile
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(MINGW)\bin\mingw32-make.exe"

