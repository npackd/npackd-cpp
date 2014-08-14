#include <QStringList>
#include <QCoreApplication>

#include "app.h"
#include "job.h"
#include "packageversion.h"
#include "wpmutils.h"
#include "clprogress.h"
#include "hrtimer.h"

App::App()
{
}

QString App::captureNpackdCLOutput(const QString& params)
{
    QString where("C:\\Users\\t\\projects\\npackd\\1.19\\npackdcl");
    QString npackdcl("C:\\Users\\t\\projects\\npackd\\1.19\\npackdcl\\npackdcl.exe");

    QStringList env;
    Job* job = new Job();
    QByteArray output = PackageVersion::executeFile(
            job, where,
            npackdcl,
            params, "Output.log", env);
    delete job;

    QString s(output);

    return s;
}

void App::pathIsFast()
{
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
    // QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("installed successfully"));
    QVERIFY(captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("is already installed in"));
    QVERIFY(captureNpackdCLOutput("rm -p io.mpv.mpv-64 -v 0.4").
            contains("removed successfully"));
}

void App::help()
{
    QVERIFY(captureNpackdCLOutput("help").contains("ncl update"));

    QVERIFY(captureNpackdCLOutput("").contains("Missing command"));
}

void App::list()
{
    QVERIFY(captureNpackdCLOutput("list").contains("Windows Installer 5.0"));

    QVERIFY(!captureNpackdCLOutput("list --bare-format").
            contains("Reading list of installed packages"));
}

void App::detect()
{
    QVERIFY(captureNpackdCLOutput("detect").
            contains("Package detection completed successfully"));
}

void App::addRemoveRepo()
{
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

