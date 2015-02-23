#include <QStringList>
#include <QCoreApplication>
#include <QDir>

#include "app.h"
#include "job.h"
#include "packageversion.h"
#include "wpmutils.h"
#include "clprogress.h"
#include "hrtimer.h"

App::App()
{
    HANDLE hToken;
    OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken);

    DWORD infoLen;
    TOKEN_ELEVATION_TYPE elevationType;
    GetTokenInformation(
            hToken, TokenElevationType,
            &elevationType, sizeof(elevationType), &infoLen);
    this->admin = elevationType == TokenElevationTypeFull;
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
            params, "Output.log", env);
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


