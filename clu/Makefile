# This is the main build file for CLU.

# Parameter: release32
PROFILE=release32

# ------------------------------------------------------------------------------

.PHONY: all printvars clean compile prep zip msi drmemory upload

SHELL:=cmd.exe

PROJECT=CLU

include ../Common.mak

ifeq ($(PROFILE),release32)
CONFIG=release
WHERE=build\32\release
endif

ifeq ($(PROFILE),release64)
CONFIG=release
WHERE=build\64\release
endif

RPAREN=)
AI=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 20$(RPAREN)")
SEVENZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 20$(RPAREN)")
PUTTY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=uk.org.greenend.chiark.sgtatham.Putty" "--versions=[0.62, 2$(RPAREN)")
EXEPROXY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=exeproxy" "--versions=[0.2, 1$(RPAREN)")
VERSION=$(shell type ..\wpmcpp\version.txt)

all:
	$(MAKE) -C tests clean PROFILE=release32
	$(MAKE) -C tests compile PROFILE=release32
	tests\build\32\release\tests -v2
	$(MAKE) -C ftests clean PROFILE=release32
	$(MAKE) -C ftests compile PROFILE=release32
	$(MAKE) clean PROFILE=release32
	$(MAKE) compile PROFILE=release32
	ftests\build\32\release\ftests -v2
	$(MAKE) upload PROFILE=release32
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
	@echo PUTTY=$(PUTTY)
	@echo WHERE=$(WHERE)
	@echo CONFIG=$(CONFIG)
	@echo EXEPROXY=$(EXEPROXY)

clean: printvars
	-rmdir /s /q $(WHERE)

$(WHERE)/zip-debug: $(WHERE)
	-mkdir $(WHERE)\zip-debug

$(WHERE)/zip:
	-mkdir $(WHERE)\zip

$(WHERE):
	-mkdir $(WHERE)

$(WHERE)/../Makefile: src/$(PROJECT).pro $(WHERE) 
	rem note how && directly follows \bin. Otherwise the path would contain a space
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(QT)\qtbase\bin\qmake.exe" ..\..\src\$(PROJECT).pro -r -spec win32-g++ CONFIG+=$(CONFIG)

compile: printvars $(WHERE) $(WHERE)/../Makefile
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(MINGW)\bin\mingw32-make.exe"

prep: compile $(WHERE)/zip
	cd $(WHERE) && copy ..\$(PROJECT)_release.map $(PROJECT)-$(VERSION).map
	copy LICENSE.txt $(WHERE)\zip
	copy $(WHERE)\$(PROJECT).exe $(WHERE)\zip\clu.exe
	"$(MINGW)\bin\strip.exe" $(WHERE)\zip\clu.exe

zip: prep
	cd $(WHERE)\zip && "$(SEVENZIP)\7z" a ..\$(PROJECT)-$(VERSION).zip *

zip-debug: compile $(WHERE)/zip-debug
	copy LICENSE.txt $(WHERE)\zip-debug
	copy $(WHERE)\$(PROJECT).exe $(WHERE)\zip-debug\$(PROJECT).exe
	copy "$(DRMINGW)\bin\exchndl.dll" $(WHERE)\zip-debug
	copy "$(DRMINGW)\bin\mgwhelp.dll" $(WHERE)\zip-debug
	copy "$(DRMINGW)\bin\dbghelp.dll" $(WHERE)\zip-debug
	copy "$(DRMINGW)\bin\symsrv.dll" $(WHERE)\zip-debug
	copy "$(DRMINGW)\bin\symsrv.yes" $(WHERE)\zip-debug
	cd $(WHERE)\zip-debug && "$(SEVENZIP)\7z" a ..\$(PROJECT)$(BITS)-debug-$(VERSION).zip *

msi: zip
	"$(AI)\bin\x86\AdvancedInstaller.com" /edit src\$(PROJECT)$(BITS).aip /SetVersion $(VERSION)
	"$(AI)\bin\x86\AdvancedInstaller.com" /build src\$(PROJECT)$(BITS).aip

ifeq (32,$(BITS))
drmemory: compile
	cd $(WHERE) && "C:\ProgramFiles\Dr._Memory\bin\drmemory.exe" -no_callstack_use_top_fp -- $(PROJECT).exe
endif

upload: zip msi
	echo 
	"$(PUTTY)\pscp.exe" -C \
		$(WHERE)/$(PROJECT)-$(VERSION).zip \
	    $(WHERE)/$(PROJECT)-$(VERSION).msi \
	    $(WHERE)/$(PROJECT)-$(VERSION).map \
		hilt2@frs.sourceforge.net:/home/pfs/project/n/np/npackd/$(PACKAGE)/

