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

    CLProgress clp;

    QStringList env;
    Job* job = clp.createJob();
    QByteArray output = PackageVersion::executeFile(
            job, where,
            npackdcl,
            params, "Output.log", env);
    delete job;

    QString s(output);
    WPMUtils::outputTextConsole(s);
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

void App::functionalTest()
{
    // QSKIP("disabled");

    QWARN("---- testing add\n");
    QVERIFY(captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("installed successfully"));

    QWARN("---- testing add twice\n");
    QVERIFY(captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("is already installed in"));

    QWARN("---- testing rm\n");
    QVERIFY(captureNpackdCLOutput("rm -p io.mpv.mpv-64 -v 0.4").
            contains("removed successfully"));

    QWARN("---- testing help\n");
    QVERIFY(captureNpackdCLOutput("help").contains("ncl update"));

    QWARN("---- testing missing command\n");
    QVERIFY(captureNpackdCLOutput("").contains("Missing command"));

    QWARN("---- testing list\n");
    QVERIFY(captureNpackdCLOutput("list").contains("Windows Installer 5.0"));

    QWARN("---- testing list\n");
    QVERIFY(!captureNpackdCLOutput("list --bare-format").
            contains("Reading list of installed packages"));

    QWARN("---- testing detect\n");
    QVERIFY(captureNpackdCLOutput("detect").
            contains("Package detection completed successfully"));

    QWARN("---- testing add-repo\n");
    QVERIFY(captureNpackdCLOutput("add-repo --url http://localhost:8083/NonExisting.xml").
            contains("The repository was added successfully"));

    QWARN("---- testing remove-repo\n");
    QVERIFY(captureNpackdCLOutput("remove-repo --url http://localhost:8083/NonExisting.xml").
            contains("The repository was removed successfully"));

    QWARN("---- testing search\n");
    QVERIFY(captureNpackdCLOutput("search -q \"c++\"").
            contains("Microsoft Visual C++"));

    QWARN("---- testing search -b\n");
    QVERIFY(!captureNpackdCLOutput("search -q \"c++\" -b").
            contains("packages found"));

    QWARN("---- testing info\n");
    QVERIFY(captureNpackdCLOutput("info -p io.mpv.mpv").
            contains("command line movie player"));

    QWARN("---- testing list-repos\n");
    QVERIFY(captureNpackdCLOutput("list-repos").
            contains("npackd.appspot.com"));

    QWARN("---- testing check\n");
    QVERIFY(captureNpackdCLOutput("check").
            contains("All dependencies are installed"));
}

