# Npackd

* Home page and repositories: https://npackd.appspot.com
* Downloads: https://github.com/tim-lebedkov/npackd/wiki/Downloads

![Npackd](http://npackd.appspot.com/Npackd.png)

Npackd (pronounced "unpacked") is an application store/package manager/marketplace for applications for Windows. It helps you to find and install software, keep your system up-to-date and uninstall it if no longer necessary. You can watch [this short video](https://www.youtube.com/watch?v=ZLJ8sv6siKQ) to better understand how it works. The process of installing and uninstalling applications is completely automated (silent or unattended installation and un-installation). There is also a [command line](https://github.com/tim-lebedkov/npackd/wiki/CommandLine) based version of Npackd which you can [install](https://github.com/tim-lebedkov/npackd/wiki/CommandLineInstallation) from the command line: 

Easy installation of the graphical user interface from the command line (64 bit):

```Batchfile
C:\> msiexec.exe /qb- /i https://bit.ly/npackd-1_20_5
```

There is also a [command line](https://github.com/tim-lebedkov/npackd/wiki/CommandLine) based version of Npackd which you can [install](https://github.com/tim-lebedkov/npackd/wiki/CommandLineInstallation) from the command line: 

```Batchfile
C:\> msiexec.exe /qb- /i https://bit.ly/npackdcl-1_20_5
```

see [What is new in Npackd](https://github.com/tim-lebedkov/npackd/wiki/ChangeLog)

## Project status
[![Build artifacts](https://ci.appveyor.com/api/projects/status/github/tim-lebedkov/npackd-cpp)](https://ci.appveyor.com/project/tim-lebedkov/npackd-cpp)
[![Build artifacts](https://scan.coverity.com/projects/4151/badge.svg?flat=1)](https://scan.coverity.com/projects/4151?tab=overview)

## News
You can follow the news on [Twitter](http://twitter.com/Npackd)

## Third party tools working with Npackd:
  * [Npackd plugin for chooie](https://github.com/TomPeters/chooie.Npackd)
  * [Puppet package provider for Npackd](http://forge.puppetlabs.com/badgerious/npackd)
  * [Npackd for Sublime Text Syntax - completions and snippets for creating Npackd XML files](https://sublime.wbond.net/packages/Npackd)

##Distribute your applications using Npackd!
You can also distribute your own applications using Npackd: either through your own repository or through the one mentioned above. All you have to do is to package your application as a ZIP file so it is accessible through HTTP and describe it as shown in RepositoryFormat. It is even easier if you already have an .msi or .exe installer. In most cases they can be reused without re-packaging.

##Main features
  * synchronizes information about installed programs with the control panel "Add or remove software" and MSI package database. Allow uninstallation of those packages. 
  * support for proxies (use the internet settings control panel to configure it)
  * password protected pages. This can be used to restrict access to your repository.
  * fast installation and uninstallation without user interaction. A typical application is installed and uninstalled in seconds (downloading the package is the most lengthy operation)
  * dependencies
  * shortcuts in the start menu are automatically created/deleted
  * multiple program versions can be installed side-by-side
  * cryptographic checksum for packages (SHA1)
  * prevents uninstallation of running programs

[![Project Stats](https://www.openhub.net/p/windows-package-manager/widgets/project_thin_badge.gif)](https://www.openhub.net/p/windows-package-manager)

<a class="addthis_button" href="http://www.addthis.com/bookmark.php?v=250&amp;username=xa-4c376eea7c4cc880"><img src="https://s7.addthis.com/static/btn/v2/lg-share-en.gif" width="125" height="16" alt="Bookmark and Share" style="border:0"/></a>

Download statistics: http://www.somsubhra.com/github-release-stats/?username=tim-lebedkov&repository=npackd-cpp

Windows is a registered trademark of Microsoft Corporation in the United States and other countries.

