# This is the main build file for Npackd. It builds both 32 bit and 64 bit
# versions.

# Parameter: release32, release64
PROFILE=release32

# ------------------------------------------------------------------------------

.PHONY: all printvars clean compile prep zip msi drmemory translations

SHELL:=cmd.exe

ifeq (32,$(findstring 32,$(PROFILE)))
QT=C:\NpackdSymlinks\com.nokia.QtDev-i686-w64-Npackd-Release-5.5
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-i686-sjlj-posix" "--versions=[4.9.2, 4.9.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-i686-w64-static" "--versions=[0.7.1, 0.7.1]")
DRMINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=drmingw" "--versions=[0.7.7, 0.7.7]")
BITS=32
PACKAGE=com.googlecode.windows-package-manager.Npackd
else
QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.5
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.9.2, 4.9.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.7.1, 0.7.1]")
DRMINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=drmingw64" "--versions=[0.7.7, 0.7.7]")
BITS=64
PACKAGE=com.googlecode.windows-package-manager.Npackd64
endif
ifeq ($(PROFILE),release32)
CONFIG=release
endif
ifeq ($(PROFILE),release64)
CONFIG=release
endif
EXEPROXY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=exeproxy" "--versions=[0.2, 0.2]")

RPAREN=)
AI=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 20$(RPAREN)")
SEVENZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 20$(RPAREN)")
PUTTY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=uk.org.greenend.chiark.sgtatham.Putty" "--versions=[0.62, 2$(RPAREN)")
DRMEMORY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=drmemory" "--versions=[1, 20$(RPAREN)")
VERSION=$(shell cmd /c type version.txt)

all:
	$(MAKE) clean PROFILE=release32
	$(MAKE) zip msi PROFILE=release32
	$(MAKE) clean PROFILE=release64
	$(MAKE) zip msi PROFILE=release64
	echo ======================= SUCCESS =======================================

printvars:
	@echo PATH=$(PATH)
	@echo CURDIR=$(CURDIR)
	@echo PROFILE=$(PROFILE)
	@echo BITS=$(BITS)
	@echo VERSION=$(VERSION)
	@echo MINGW=$(MINGW)
	@echo DRMINGW=$(DRMINGW)
	@echo QT=$(QT)
	@echo AI=$(AI)
	@echo SEVENZIP=$(SEVENZIP)
	@echo QUAZIP=$(QUAZIP)
	@echo PUTTY=$(PUTTY)
	@echo CONFIG=$(CONFIG)
	@echo EXEPROXY=$(EXEPROXY)
ifndef PROFILE
	$(error PROFILE is not defined)
endif
ifndef BITS
	$(error BITS is not defined)
endif
ifndef VERSION
	$(error VERSION is not defined)
endif
ifndef QT
	$(error QT is not defined)
endif
ifndef MINGW
	$(error MINGW is not defined)
endif
ifndef DRMINGW
	$(error DRMINGW is not defined)
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
ifndef EXEPROXY
	$(error EXEPROXY is not defined)
endif

zip-debug:
	-mkdir zip-debug

zip: 
	-mkdir zip

# rem note how && directly follows \bin. Otherwise the path would contain a space
../Makefile: src/wpmcpp.pro
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd .. && "$(QT)\qtbase\bin\qmake.exe" ..\..\src\wpmcpp.pro -r -spec win32-g++ CONFIG+=$(CONFIG)

compile: printvars translations ../Makefile
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd .. && "$(MINGW)\bin\mingw32-make.exe"

prep: compile /zip
	copy ..\wpmcpp_release.map Npackd$(BITS)-$(VERSION).map
	copy LICENSE.txt zip
	copy CrystalIcons_LICENSE.txt zip
	copy wpmcpp.exe zip\npackdg.exe
	"$(MINGW)\bin\strip.exe" zip\npackdg.exe
	copy "$(EXEPROXY)\exeproxy.exe" zip

zip-debug: compile zip-debug
	copy LICENSE.txt zip-debug
	copy CrystalIcons_LICENSE.txt zip-debug
	copy wpmcpp.exe zip-debug\npackdg.exe
	copy "$(DRMINGW)\bin\exchndl.dll" zip-debug
	copy "$(DRMINGW)\bin\mgwhelp.dll" zip-debug
	copy "$(DRMINGW)\bin\dbghelp.dll" zip-debug
	copy "$(DRMINGW)\bin\symsrv.dll" zip-debug
	copy "$(DRMINGW)\bin\symsrv.yes" zip-debug
	cd zip-debug && "$(SEVENZIP)\7z" a ..\Npackd$(BITS)-debug-$(VERSION).zip *

zip: prep
	cd zip && "$(SEVENZIP)\7z" a ..\Npackd$(BITS)-$(VERSION).zip * -mx9

msi: prep
	"$(AI)\bin\x86\AdvancedInstaller.com" /edit src\wpmcpp$(BITS).aip /SetVersion $(VERSION)
	"$(AI)\bin\x86\AdvancedInstaller.com" /build src\wpmcpp$(BITS).aip

ifeq (32,$(BITS))
# memcheck: compile
# 	"$(DRMEMORY)\bin\drmemory.exe" -light -count_leaks -no_follow_children -no_callstack_use_top_fp -- wpmcpp.exe

drmemory: compile
	"$(DRMEMORY)\bin\drmemory.exe" -no_follow_children -report_leak_max 100000 -no_callstack_use_top_fp -- wpmcpp.exe
endif

translations:
	"$(QT)\qtbase\bin\lupdate.exe" -locations none src\wpmcpp.pro
	"$(QT)\qtbase\bin\lrelease.exe" src\wpmcpp.pro

