# This is the main build file for Npackd. It builds both 32 bit and 64 bit
# versions.

# Parameter: release32, gprof32, release64, gprof64
PROFILE=release32

# ------------------------------------------------------------------------------

.PHONY: all printvars clean compile prep zip msi gprof drmemory upload translations

SHELL:=cmd.exe

ifeq (32,$(findstring 32,$(PROFILE)))
QT=C:\NpackdSymlinks\com.nokia.QtDev-i686-w64-Npackd-Release-5.2.1
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-i686-sjlj-posix" "--versions=[4.8.2, 4.8.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-i686-w64-static" "--versions=[0.6.2, 0.6.2]")
BITS=32
PACKAGE=com.googlecode.windows-package-manager.Npackd
else
QT=C:\NpackdSymlinks\com.nokia.QtDev-x86_64-w64-Npackd-Release-5.2.1
MINGW=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=mingw-w64-x86_64-seh-posix" "--versions=[4.8.2, 4.8.2]")
QUAZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=quazip-dev-x86_64-w64-static" "--versions=[0.6.2, 0.6.2]")
BITS=64
PACKAGE=com.googlecode.windows-package-manager.Npackd64
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
AI=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=com.advancedinstaller.AdvancedInstallerFreeware" "--versions=[10, 20$(RPAREN)")
SEVENZIP=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.7-zip.SevenZIP" "--versions=[9, 10$(RPAREN)")
MINGWUTILS=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=org.mingw.MinGWUtilities" "--versions=[0.3, 0.3]")
PUTTY=$(shell "$(NPACKD_CL)\npackdcl.exe" "path" "--package=uk.org.greenend.chiark.sgtatham.Putty" "--versions=[0.62, 2$(RPAREN)")
VERSION=$(shell cmd /c type version.txt)

all:
	$(MAKE) clean PROFILE=release32
	$(MAKE) upload PROFILE=release32
	$(MAKE) clean PROFILE=release64
	$(MAKE) upload PROFILE=release64
	echo ======================= SUCCESS =======================================

printvars:
	@echo PATH=$(PATH)
	@echo PROFILE=$(PROFILE)
	@echo BITS=$(BITS)
	@echo VERSION=$(VERSION)
	@echo MINGW=$(MINGW)
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
ifndef MINGWUTILS
	$(error MINGWUTILS is not defined)
endif
ifndef PUTTY
	$(error PUTTY is not defined)
endif

clean: printvars
	-rmdir /s /q $(WHERE)

$(WHERE)/zip:
	-mkdir $(WHERE)\zip

$(WHERE):
	-mkdir $(WHERE)

# rem note how && directly follows \bin. Otherwise the path would contain a space
$(WHERE)/../Makefile: src/wpmcpp.pro $(WHERE) 
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(QT)\qtbase\bin\qmake.exe" ..\..\src\wpmcpp.pro -r -spec win32-g++ CONFIG+=$(CONFIG)

compile: printvars $(WHERE) $(WHERE)/../Makefile
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(MINGW)\bin\mingw32-make.exe" -j 3

prep: compile translations $(WHERE)/zip
	cd $(WHERE) && copy ..\wpmcpp_release.map Npackd$(BITS)-$(VERSION).map
	set path=$(MINGW)\bin&& "$(QT)\qtbase\bin\lrelease.exe" ..\..\..\src\wpmcpp.pro
	copy src\wpmcpp_es.qm $(WHERE)\zip\npackdg_es.qm
	copy src\wpmcpp_ru.qm $(WHERE)\zip\npackdg_ru.qm
	copy src\wpmcpp_fr.qm $(WHERE)\zip\npackdg_fr.qm
	copy src\wpmcpp_de.qm $(WHERE)\zip\npackdg_de.qm
	copy LICENSE.txt $(WHERE)\zip
	copy CrystalIcons_LICENSE.txt $(WHERE)\zip
	copy $(WHERE)\wpmcpp.exe $(WHERE)\zip\npackdg.exe
	"$(MINGW)\bin\strip.exe" $(WHERE)\zip\npackdg.exe
ifeq (32,$(BITS))
	copy "$(MINGWUTILS)\bin\exchndl.dll" $(WHERE)\zip
endif

zip: prep
	cd $(WHERE)\zip && "$(SEVENZIP)\7z" a ..\Npackd$(BITS)-$(VERSION).zip *

msi: prep
	"$(AI)\bin\x86\AdvancedInstaller.com" /edit src\wpmcpp$(BITS).aip /SetVersion $(VERSION)
	"$(AI)\bin\x86\AdvancedInstaller.com" /build src\wpmcpp$(BITS).aip

gprof: $(WHERE)\wpmcpp.exe $(WHERE)\gmon.out
# use --no-demangle to find out a mangled function name
# -f _ZN12DBRepository8updateF5EP3Job
# -qDBRepository::updateF5(Job*) 
	$(MINGW)\bin\gprof.exe -a -F _ZN12DBRepository8updateF5EP3Job $(WHERE)\wpmcpp.exe $(WHERE)\gmon.out >> GProf.txt

ifeq (32,$(BITS))
drmemory: compile
	cd $(WHERE) && "C:\ProgramFiles\Dr._Memory\bin\drmemory.exe" -no_callstack_use_top_fp -- wpmcpp.exe
endif

upload: zip msi
	echo 
	"$(PUTTY)\pscp.exe" -C \
	    $(WHERE)/Npackd$(BITS)-$(VERSION).zip \
	    $(WHERE)/Npackd$(BITS)-$(VERSION).msi \
	    $(WHERE)/Npackd$(BITS)-$(VERSION).map \
		hilt2,npackd@frs.sourceforge.net:/home/pfs/project/n/np/npackd/$(PACKAGE)/

translations:
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(QT)\qtbase\bin\lupdate.exe" ..\..\src\wpmcpp.pro
	set path=$(MINGW)\bin&&set quazip_path=$(QUAZIP)&& cd $(WHERE)\.. && "$(QT)\qtbase\bin\lrelease.exe" ..\..\src\wpmcpp.pro

