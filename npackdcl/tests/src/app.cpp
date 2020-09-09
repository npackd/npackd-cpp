#include <limits>
#include <math.h>
#include <memory>

#include <QRegExp>
#include <QProcess>

#include "app.h"
#include "wpmutils.h"
#include "commandline.h"
#include "downloader.h"
#include "installedpackages.h"
#include "installedpackageversion.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "hrtimer.h"

void App::test()
{
    Version a;
    Version b;
    b.setVersion("1.0.0");
    QVERIFY(a == b);

    a.setVersion("4.5.6.7.8.9.10");
    QVERIFY(a.getVersionString() == "4.5.6.7.8.9.10");

    a.setVersion("1.1");
    QVERIFY(a == Version(1, 1));

    a.setVersion("5.0.0.1");
    Version c(a);
    QVERIFY(c.getVersionString() == "5.0.0.1");

    Version* d = new Version();
    d->setVersion(7, 8, 9, 10);
    delete d;

    a.setVersion(1, 17);
    QVERIFY(a.getVersionString() == "1.17");

    a.setVersion(2, 18, 3);
    QVERIFY(a.getVersionString() == "2.18.3");

    a.setVersion(3, 1, 3, 8);
    QVERIFY(a.getVersionString() == "3.1.3.8");

    a.setVersion("17.2.8.4");
    a.prepend(8);
    a.prepend(38);
    a.prepend(0);
    QVERIFY(a.getVersionString() == "0.38.8.17.2.8.4");

    a.setVersion("2.8.3");
    QVERIFY(a.getVersionString(7) == "2.8.3.0.0.0.0");

    a.setVersion("17.2");
    QVERIFY(a.getNParts() == 2);

    a.setVersion("8.4.0.0.0");
    a.normalize();
    QVERIFY(a.getVersionString() == "8.4");
    QVERIFY(a.isNormalized());

    a.setVersion("2.8.7.4.8.9");
    b.setVersion("2.8.6.4.8.8");
    QVERIFY(a > b);
}

void App::testInstalledPackages()
{
    std::unique_ptr<InstalledPackages> ip(new InstalledPackages());

    QList<InstalledPackageVersion*> packages = ip->getAll();
    QVERIFY(packages.size() == 0);
    qDeleteAll(packages);

    InstalledPackageVersion* ipv = ip->find("test", Version(1, 2));
    QVERIFY(ipv == nullptr);

    QString err = ip->setPackageVersionPath(
            "test", Version(1, 2), "C:\\test", false);
    QVERIFY(err == "");

    ipv = ip->find("test", Version(1, 2));
    QVERIFY(ipv != nullptr);
    QVERIFY(ipv->package == "test");
    QVERIFY(ipv->version == Version(1, 2));

    ipv = ip->findOwner("C:\\test");
    QVERIFY(ipv != nullptr);
    QVERIFY(ipv->package == "test");
    QVERIFY(ipv->version == Version(1, 2));

    ipv = ip->findOwner("C:\\test\\abc");
    QVERIFY(ipv != nullptr);
    QVERIFY(ipv->package == "test");
    QVERIFY(ipv->version == Version(1, 2));

    packages = ip->getAll();
    QVERIFY(packages.size() == 1);
    qDeleteAll(packages);

    packages = ip->getByPackage("test");
    QVERIFY(packages.size() == 1);
    qDeleteAll(packages);

    packages = ip->getByPackage("test2");
    QVERIFY(packages.size() == 0);
    qDeleteAll(packages);

    QStringList paths = ip->getAllInstalledPackagePaths();
    QVERIFY(paths.size() == 1);
    QVERIFY(paths.at(0) == "C:\\test");

    QVERIFY(ip->getPath("test", Version(1, 2)) == "C:\\test");

    QVERIFY(ip->isInstalled("test", Version(1, 2)));

    ipv = ip->getNewestInstalled("test");
    QVERIFY(ipv != nullptr);
    QVERIFY(ipv->package == "test");
    QVERIFY(ipv->version == Version(1, 2));

    Dependency d;
    d.package = "test";
    d.setVersions("[1, 2)");
    QVERIFY(ip->isInstalled(d));
}

void App::testCommandLine()
{
    QString err;
    QStringList params = WPMUtils::parseCommandLine(
            "\"C:\\Program Files (x86)\\InstallShield Installation Information\\{96D0B6C6-5A72-4B47-8583-A87E55F5FE81}\\setup.exe\" -runfromtemp -l0x0007 -removeonly",
            &err);

    QVERIFY(err.isEmpty());
    QVERIFY(params.count() == 4);
    QVERIFY2(params.at(0) == "C:\\Program Files (x86)\\InstallShield Installation Information\\{96D0B6C6-5A72-4B47-8583-A87E55F5FE81}\\setup.exe",
             qPrintable(params.at(0)));
}

void App::testCopyDirectory()
{
    QString from = QDir::currentPath();
    QString to = from + "_copy";
    QString to2 = from + "_copy2";

    qCDebug(npackd) << from << to;

    QVERIFY2(WPMUtils::copyDirectory(from, to),
             qPrintable(QString("directory copying %1 to %2").arg(from).arg(to)));

    // lock the directory
    QProcess p;
    //p.start("cmd", QStringList() << "/K" << "cd" << "\"" + to + "\"");
    p.start("cmd /K cd \"" + to + "\"");

    // wait till the "cd" command gets executed
    Sleep(5000);

    Job* job = new Job("Rename directory");
    WPMUtils::renameDirectory(job, to, to2);
    QVERIFY2(job->getErrorMessage().isEmpty(),
             qPrintable(job->getTitle() + ":" + job->getErrorMessage()));
    delete job;

    QDir d;
    QVERIFY2(d.exists(to), qPrintable("1" + to));
    QVERIFY2(d.exists(to2), qPrintable("1" + to2));

    // unlock the directory
    p.kill();
    p.waitForFinished();

    job = new Job("Deleting directory to");
    WPMUtils::removeDirectory(job, to, true);
    QVERIFY2(job->getErrorMessage().isEmpty(), qPrintable(job->getTitle() + ":" + job->getErrorMessage()));
    QVERIFY2(job->isCompleted(), "job not completed");
    delete job;

    d.refresh();
    QVERIFY2(!d.exists(to), qPrintable("2" + to));
    QVERIFY2(d.exists(to2), qPrintable("2" + to2));

    job = new Job("Deleting directory to2");
    WPMUtils::removeDirectory(job, to2, true);
    QVERIFY2(job->getErrorMessage().isEmpty(), qPrintable(job->getTitle() + ":" + job->getErrorMessage()));
    QVERIFY2(job->isCompleted(), "job not completed");
    delete job;

    d.refresh();
    QVERIFY2(!d.exists(to), qPrintable("3" + to));
    QVERIFY2(!d.exists(to2), qPrintable("3" + to2));
}

void App::testNormalizePath()
{
    QCOMPARE(WPMUtils::normalizePath("../", false), "..");
}
