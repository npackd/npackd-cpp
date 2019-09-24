[![Build artifacts](https://ci.appveyor.com/api/projects/status/github/tim-lebedkov/npackd-cpp)](https://ci.appveyor.com/project/tim-lebedkov/npackd-cpp)
[![Coverity](https://scan.coverity.com/projects/4151/badge.svg?flat=1)](https://scan.coverity.com/projects/4151?tab=overview)
[![License](http://img.shields.io/badge/license-GPL%203.0-blue.svg?style=flat)](http://choosealicense.com/licenses/gpl-3.0/)

# Npackd

_(pronounced "unpacked") is an application store/package manager/marketplace for Windows_

* Home page and repositories: https://www.npackd.org

![Npackd](http://www.npackd.org/Npackd.png)

It helps you to find and install software, keep your system up-to-date and uninstall it if no longer necessary. You can watch [this short video](https://www.youtube.com/watch?v=ZLJ8sv6siKQ) to better understand how it works. The process of installing and uninstalling applications is completely automated (silent or unattended installation and un-installation). There is also a [command line](https://github.com/tim-lebedkov/npackd/wiki/CommandLine) based version of Npackd which you can [install](https://github.com/tim-lebedkov/npackd/wiki/CommandLineInstallation) from the command line: 

Easy installation of the graphical user interface from the command line (64 bit):

```Batchfile
C:\> msiexec.exe /qb- /i https://bit.ly/npackd64-1_25
```

There is also a [command line](https://github.com/tim-lebedkov/npackd/wiki/CommandLine) based version of Npackd which you can [install](https://github.com/tim-lebedkov/npackd/wiki/CommandLineInstallation) from the command line: 

```Batchfile
C:\> msiexec.exe /qb- /i https://bit.ly/npackdcl32-1_25
```

see [What is new in Npackd](https://github.com/tim-lebedkov/npackd/wiki/ChangeLog)

## News
You can follow the news on [Twitter](http://twitter.com/Npackd) or via the [Forum](https://groups.google.com/forum/#!forum/npackd)

## Main features
  * synchronizes information about installed programs with the control panel "Add or remove software" and MSI package database. Allow uninstallation of those packages. 
  * support for proxies (use the internet settings control panel to configure it)
  * password protected pages. This can be used to restrict access to your repository.
  * fast installation and uninstallation without user interaction. A typical application is installed and uninstalled in seconds (downloading the package is the most lengthy operation)
  * dependencies
  * shortcuts in the start menu are automatically created/deleted
  * multiple program versions can be installed side-by-side
  * cryptographic checksum for packages (SHA1 and SHA-256)
  * closes running applications if necessary
  * runs on [ReactOS](https://www.youtube.com/watch?v=m7o5e-RhY64) and under [Linux/Wine](https://groups.google.com/forum/#!searchin/npackd/wine%7Csort:relevance/npackd/9LSMzh_0LnQ/-LFL_nKJDAAJ)

## Statistics
[![Project Stats](https://www.openhub.net/p/windows-package-manager/widgets/project_thin_badge.gif)](https://www.openhub.net/p/windows-package-manager)

Download statistics: http://www.somsubhra.com/github-release-stats/?username=tim-lebedkov&repository=npackd-cpp

Windows is a registered trademark of Microsoft Corporation in the United States and other countries.

