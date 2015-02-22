# Parameter: release32, gprof32, release64, gprof64
PROFILE=release32

# ------------------------------------------------------------------------------

.PHONY: all printvars clean compile prep zip gprof drmemory upload

SHELL:=cmd.exe

PROJECT=exeproxy

ifeq (32,$(findstring 32,$(PROFILE)))
QT=C:\NpackdSymlinks\com.nokia.QtDev-i686-w64-Npackd-Release-5.2.1
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-i686-sjlj-posix" "--versions=[4.8.2, 4.8.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-i686-w64-static" "--versions=[0.6.2, 0.6.2]")
BITS=32
PACKAGE=exeproxy
else
QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.2.1
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.8.2, 4.8.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.6.2, 0.6.2]")
BITS=64
endif
ifeq ($(PROFILE),release32)
CONFIG=release
WHERE=build\32\release
endif
ifeq ($(PROFILE),release64)
CONFIG=release
WHERE=build\64\release
endif
ifeq ($(PROFILE),gprof32)
CONFIG=gprof
WHERE=build\32\gprof
endif
ifeq ($(PROFILE),gprof64)
CONFIG=gprof
WHERE=build\64\gprof
endif

RPAREN=)
AI=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 11$(RPAREN)")
SEVENZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 10$(RPAREN)")
MINGWUTILS=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.mingw.MinGWUtilities" "--versions=[0.3, 0.3]")
PUTTY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=uk.org.greenend.chiark.sgtatham.Putty" "--versions=[0.62, 2$(RPAREN)")
VERSION=0.2

all:
	$(MAKE) clean PROFILE=release32
	$(MAKE) zip PROFILE=release32
	@echo ======================= SUCCESS =======================================

printvars:
	@echo PROFILE=$(PROFILE)
	@echo BITS=$(BITS)
	@echo VERSION=$(VERSION)
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
ifndef VERSION
	$(error VERSION is not defined)
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
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(MINGW)\bin\mingw32-make.exe" -j 3

prep: compile $(WHERE)/zip
	cd $(WHERE) && copy ..\$(PROJECT)_release.map $(PROJECT)-$(VERSION).map
	copy LICENSE.txt $(WHERE)\zip
	copy $(WHERE)\$(PROJECT).exe $(WHERE)\zip\$(PROJECT).exe
	"$(MINGW)\bin\strip.exe" $(WHERE)\zip\$(PROJECT).exe

zip: prep
	cd $(WHERE)\zip && "$(SEVENZIP)\7z" a ..\$(PROJECT)-$(VERSION).zip *

gprof: $(WHERE)\$(PROJECT).exe $(WHERE)\gmon.out
# use --no-demangle to find out a mangled function name
# -f _ZN12DBRepository8updateF5EP3Job
# -qDBRepository::updateF5(Job*) 
	$(MINGW)\bin\gprof.exe -a -F _ZN12DBRepository8updateF5EP3Job $(WHERE)\$(PROJECT).exe $(WHERE)\gmon.out >> GProf.txt

ifeq (32,$(BITS))
drmemory: compile
	cd $(WHERE) && "C:\ProgramFiles\Dr._Memory\bin\drmemory.exe" -no_callstack_use_top_fp -- $(PROJECT).exe
endif

upload: zip 
	"$(PUTTY)\pscp.exe" -C \
		$(WHERE)/$(PROJECT)-$(VERSION).zip \
	    $(WHERE)/$(PROJECT)-$(VERSION).map \
		hilt2@frs.sourceforge.net:/home/pfs/project/n/np/npackd/$(PACKAGE)/

