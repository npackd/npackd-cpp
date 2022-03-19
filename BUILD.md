# How to build

This file describes the steps necessary to build 64 bit version of Npackd 
(GUI, command line or both depending which CMakeLists.txt you use) using MSYS2. 
You can build the 32 bit version replacing the packages mentioned below with 
their 32 bit versions.

## Setup a virtual machine under Linux if Windows is not available ##

- Download and install VirtualBox from https://www.virtualbox.org/
- Choose "MSEdge on Win10 (x64) Stable 1809", "Virtual Box" on 
   https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/, 
   download the virtual machine disk image, unzip the file.
- Start "Oracle VM VirtualBox Manager"
   choose "File", "Import appliance..."
   navigate to "MSEdge-Win10.ova", use the default settings, press "Import"
- Edit the VM definition
  - (optional) "General", "Extended", set "Common clipboard" to "bidirectional"
  - (optional) "System", "Main memory", set to 8192 MB, 
  - (optional) "System", "Processor", set to 8 CPUs
  - (optional) "Audio", "Extended", check "Audio output"
- "Tools", "Hard disks", "Properties", resize the disk in VirtualBox to 100 GiB ()
- Start the VM. Password for IEUser is Passw0rd!, 
- restart the VM so that the clipboard starts to work
- change the password: "net user ieuser newpassword" as administrator, restart VM

## Configure Windows (optional)
- (optional) Start netplwiz, enable and disable password on login. Enter new password. Restart VM.
- (optional) change the desktop resolution to 1280x960
- (optional) Start "Region & language" settings, change the language, restart the VM
- (optional) start diskmgmt.msc, extend the hard disk partition

## Install necessary software

- Start cmd.exe as administrator
```bat
msiexec.exe /qb- /i http://bit.ly/npackdcl64-1_26_9
set path=c:\Program Files\NpackdCL;%path%
ncl set-repo -u https://www.npackd.org/rep/zip?tag=libs -u https://www.npackd.org/rep/zip?tag=stable -u https://www.npackd.org/rep/zip?tag=stable64
ncl detect
ncl add -p com.googlecode.windows-package-manager.Npackd64 -p windows10debloater -p qt-creator64 -p vs-code64 -p com.microsoft.ProcessExplorer -p org.7-zip.SevenZIP64 -p nircmd64 -p com.advancedinstaller.AdvancedInstallerFreeware
ncl add -p astrogrep -p dbeaver64 -p drmemory -p firefox64 -p com.googlecode.gitextensions.GitExtensions -p kdiff3-64 -p com.lockhunter.LockHunter64 -p notepadpp64 -p org.cmake.CMake
mkdir c:\builds
ncl add -p quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static --file c:\Builds\quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static
```
- (optional) Start PowerShell as administrator
```powershell
Set-ExecutionPolicy Unrestricted -Force 
cd 'C:\Program Files\Windows10Debloater\'
.\Windows10DebloaterGUI.ps1
```
	Click "Remove all Bloatware"
	Click "Edge PDF/disable"
	Click "Disable telemetry/tasks"
	Click "Remove Bloatware regkeys"
```bat
ncl add -p msys2_64 --file c:\msys64
```
- Start c:\msys64\mingw64.exe
```bash
pacman -Syu --noconfirm
pacman -Su --noconfirm

pacman -S --noconfirm mingw-w64-x86_64-libtool mingw64/mingw-w64-x86_64-jasper mingw64/mingw-w64-x86_64-qt5 mingw64/mingw-w64-x86_64-icu mingw64/mingw-w64-x86_64-zstd mingw64/mingw-w64-x86_64-quazip
pacman -S --noconfirm mingw-w64-x86_64-ninja mingw64/mingw-w64-x86_64-zstd
pacman -S --noconfirm mingw-w64-x86_64-toolchain
```
- Start Git Extensions and configure "C:\Program Files\KDiff3\bin\kdiff3.exe" as diff and merge tool,
    tim.lebedkov@gmail.com as the email and tim-lebedkov as user name.
	Clone https://github.com/tim-lebedkov/npackd-cpp.git to C:\Users\IEUser\Documents\npackd-cpp

## Configure

```bat
set path=C:\msys64\mingw64\bin;C:\Program Files\CMake\bin
set CMAKE_PREFIX_PATH=C:\msys64\mingw64\x86_64-w64-mingw32
mkdir c:\builds\npackd-dyn-minsizerel
cd c:\builds\npackd-dyn-minsizerel
cmake C:\Users\IEUser\Documents\npackd-cpp -G "Ninja" -DCMAKE_BUILD_TYPE=MinSizeRel -DNPACKD_FORCE_STATIC:BOOL=OFF
```

## Build

```bat
ninja
```
## Configure the Visual Studio Code (optional, recommended)
 * add "C/C++", "Doxygen Documentation Generator", "CMake Tools" extensions
 * "CMake: Edit user-local CMake kits"
```json
[{
    "name": "GCC",
    "compilers": {
        "C": "C:\\msys64\\mingw64\\bin\\gcc.exe",
        "CXX": "C:\\msys64\\mingw64\\bin\\g++.exe"
    }
}]
``` 
 * "CMake: configure"
 * "CMake: build"
 * add "C:\msys64\mingw64\bin" to PATH
 * install "settings.json" and "keybindings.json" under "C:\Users\IEUser\AppData\Roaming\Code\User"

## Configure the Qt Creator IDE (optional)

* (optional) Start Qt Creator as administrator
    * "File"/"Manage sessions"/"Reload session on startup"
    * "File"/"Open file or project"/"CMakeLists.txt"
    * "Projects"/"Import existing build" in Qt creator, choose "C:\builds\npackd-dyn-minsizerel"
    * Define the C++ compiler

## Performance profiling (optional)

The best results can be achieved with Very Sleepy performance profiler:

```bat
ncl add -p very-sleepy
```

You can also use GProf for performance profiling:

```bat
set path=C:\msys64\mingw64\bin;C:\Program Files\CMake\bin
set CMAKE_PREFIX_PATH=C:\msys64\mingw64\x86_64-w64-mingw32;C:\Builds\quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static
mkdir c:\builds\npackd-dyn-minsizerel
cd c:\builds\npackd-dyn-minsizerel
cmake C:\Users\IEUser\Documents\npackd-cpp -DCMAKE_CXX_FLAGS=-pg -DCMAKE_EXE_LINKER_FLAGS=-pg -DCMAKE_SHARED_LINKER_FLAGS=-pg -G "Ninja" -DCMAKE_BUILD_TYPE=MinSizeRel -DNPACKD_FORCE_STATIC:BOOL=OFF
npackdcl detect
gprof npackdcl.exe gmon.out > analysis.txt
```
