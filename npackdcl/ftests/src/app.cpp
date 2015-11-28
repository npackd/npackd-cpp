#include <shlobj.h>

#include <QStringList>
#include <QCoreApplication>
#include <QDir>

#include "app.h"
#include "job.h"
#include "packageversion.h"
#include "wpmutils.h"
#include "clprogress.h"
#include "hrtimer.h"
#include "dbrepository.h"

App::App()
{
    this->admin = true;
}

QString App::captureNpackdCLOutput(const QString& params)
{
    QDir d(WPMUtils::getExeDir() + "\\..\\..\\..\\..\\build\\32\\release");

    QString where = d.absolutePath();
    QString npackdcl = where + "\\npackdcl.exe";

    QStringList env;
    Job* job = new Job();
    PackageVersion::executeFile(
            job, where,
            npackdcl,
            params, "Output.log", env, false);
    // qDebug() << job->getErrorMessage();
    delete job;

    QFile f("Output.log");
    f.open(QFile::ReadOnly);
    QByteArray output = f.readAll();
    f.close();

    QString s(output);

    return s;
}

void App::pathIsFast()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("installed successfully"));

    HRTimer t(2);
    t.time(0);
    QVERIFY(captureNpackdCLOutput("path -p io.mpv.mpv-64").
            contains("mpv_64-bit"));
    t.time(1);
    QVERIFY2(t.getTime(1) < 0.1, qPrintable(QString("%1").arg(t.getTime(1))));
}

void App::addRemove()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully");

    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer").
            contains("installed successfully"));
    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer").
            contains("installed successfully"));
    QVERIFY(captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully"));
}

void App::addToDir()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully");

    QString output = captureNpackdCLOutput(
            "add -p active-directory-explorer -f \"C:\\Program Files\\ADE\"");
    QVERIFY2(output.contains(
            "installed successfully in C:\\Program Files\\ADE"),
            output.toLatin1());

    output = captureNpackdCLOutput("add -p active-directory-explorer");
    QVERIFY2(output.contains(
            "installed successfully in C:\\Program Files\\ADE"),
            output.toLatin1());

    output = captureNpackdCLOutput(
            "add -p active-directory-explorer -f \"C:\\Program Files\\ADE2\"");
    QVERIFY2(output.contains(
            "is already installed in C:\\Program Files\\ADE"),
            output.toLatin1());

    output = captureNpackdCLOutput("rm -p active-directory-explorer");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::updateKeepDirectories()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p org.areca-backup.ArecaBackup");

    QString output = captureNpackdCLOutput(
            "add -p org.areca-backup.ArecaBackup -v 7.3.5 -f \"C:\\Program Files\\ArecaBackup\"");
    QVERIFY2(output.contains(
            "installed successfully in C:\\Program Files\\ArecaBackup"),
            output.toLatin1());

    output = captureNpackdCLOutput("update -p org.areca-backup.ArecaBackup -k");
    QVERIFY2(output.contains(
            "The packages were updated successfully"),
            output.toLatin1());

    QVERIFY(captureNpackdCLOutput("path -p org.areca-backup.ArecaBackup").
            trimmed() == "C:\\Program Files\\ArecaBackup");

    output = captureNpackdCLOutput("rm -p org.areca-backup.ArecaBackup");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::updateInstall()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("set-install-dir -f \"C:\\Program Files\"");
    captureNpackdCLOutput("rm -p org.areca-backup.ArecaBackup");

    QString output = captureNpackdCLOutput(
            "update -p org.areca-backup.ArecaBackup");
    QVERIFY2(output.contains(
            "No installed version found for the package Areca"),
            output.toLatin1());

    output = captureNpackdCLOutput("update -p org.areca-backup.ArecaBackup -i");
    QVERIFY2(output.contains(
            "The packages were updated successfully"),
            output.toLatin1());

    QVERIFY(captureNpackdCLOutput("path -p org.areca-backup.ArecaBackup").
            trimmed() == "C:\\Program Files\\Areca");

    output = captureNpackdCLOutput("rm -p org.areca-backup.ArecaBackup");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::addRemoveRunning()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer").
            contains("installed successfully"));
    QString dir = captureNpackdCLOutput("path -p active-directory-explorer").
            trimmed();
    QProcess::execute("cmd.exe /C start \"\" \"" + dir + "\\ADExplorer.exe\"");
    Sleep(5000);
    QVERIFY(captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully"));
}

void App::addDoesntProduceDetected()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p net.poedit.POEdit -v 1.7.3.1");

    captureNpackdCLOutput("set-install-dir -f \"C:\\Program Files\"");

    QVERIFY(captureNpackdCLOutput("add -p net.poedit.POEdit -v 1.7.3.1").
            contains("installed successfully"));

    QString s = captureNpackdCLOutput("info -p net.poedit.POEdit");
    int p = s.indexOf("C:\\Program Files\\Poedit");
    QVERIFY2(p > 0, s.toLatin1());

    int p2 = s.indexOf("C:\\Program Files\\Poedit", p + 1);
    QVERIFY2(p2 < 0, s.toLatin1());
}

void App::addWithoutVersion()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer").
            contains("installed successfully"));

    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer").
            contains("installed successfully"));

    QVERIFY(captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully"));
}

void App::help()
{
    QVERIFY(captureNpackdCLOutput("help").contains("ncl update"));

    QVERIFY(captureNpackdCLOutput("").contains("Missing command"));
}

void App::which()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer").
            contains("installed successfully"));
    QString dir = captureNpackdCLOutput("path -p active-directory-explorer").trimmed();
    QDir d;
    QVERIFY2(d.exists(dir), dir.toLatin1());

    QVERIFY(captureNpackdCLOutput("which -f \"" + dir + "\"").
            contains("active-directory-explorer"));
    QVERIFY(captureNpackdCLOutput("which -f \"" + dir + "\\ADExplorer.exe\"").
            contains("active-directory-explorer"));
    QVERIFY(captureNpackdCLOutput("which -f \"" + dir + "\\NonExisting.exe\"").
            contains("active-directory-explorer"));
    QVERIFY(captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully"));
}

void App::list()
{
    QVERIFY(captureNpackdCLOutput("list").contains("Windows Installer 5.0"));

    QVERIFY(!captureNpackdCLOutput("list --bare-format").
            contains("Reading list of installed packages"));
}

void App::detect()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("detect").
            contains("Package detection completed successfully"));
}

void App::addRemoveRepo()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add-repo --url http://localhost:8083/NonExisting.xml").
            contains("The repository was added successfully"));

    QVERIFY(captureNpackdCLOutput("remove-repo --url http://localhost:8083/NonExisting.xml").
            contains("The repository was removed successfully"));
}

void App::search()
{
    QVERIFY(captureNpackdCLOutput("search -q \"c++\"").
            contains("Microsoft Visual C++"));

    QVERIFY(!captureNpackdCLOutput("search -q \"c++\" -b").
            contains("packages found"));
}

void App::info()
{
    QVERIFY(captureNpackdCLOutput("info -p io.mpv.mpv").
            contains("command line movie player"));
}

void App::listRepos()
{
    QVERIFY(captureNpackdCLOutput("list-repos").
            contains("npackd.appspot.com"));
}

void App::check()
{
    QVERIFY(captureNpackdCLOutput("check").
            contains("All dependencies are installed"));
}


