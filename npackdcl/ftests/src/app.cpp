#include <shlobj.h>

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>

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

void App::init()
{
    QString rep = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() +
            "\\Rep.xml").toString();
    captureNpackdCLOutput("set-repo "
            "-u " + rep + " "
            "-u https://www.npackd.org/rep/recent-xml "
            "-u https://www.npackd.org/rep/xml?tag=stable "
            "-u https://www.npackd.org/rep/xml?tag=stable64 "
            "-u https://www.npackd.org/rep/xml?tag=libs ");
    captureNpackdCLOutput("detect");
}

QString App::captureNpackdCLOutput(const QString& params)
{
    QDir d(WPMUtils::getExeDir() + "\\..\\..\\install");
    if (!d.exists("npackdcl.exe"))
        d.cd(WPMUtils::getExeDir() + "\\..");

    QString where = d.absolutePath();
    QString npackdcl = where + "\\npackdcl.exe";

    return captureOutput(npackdcl, params, where);
}

QString App::captureOutput(const QString& program, const QString& params,
        const QString& where)
{
    qCDebug(npackd) << program << params;

    std::vector<QString> env;
    Job* job = new Job();
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    WPMUtils::executeFile(
            job, where,
            program,
            params, &buffer, env);
    qCDebug(npackd) << job->getErrorMessage();
    delete job;

    QByteArray output = buffer.data();

    QString s(output);

    WPMUtils::writeln(s);

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
    QVERIFY2(t.getTime(1) < 0.2, qPrintable(QString("%1").arg(t.getTime(1))));
}

void App::pathVersion()
{
    if (!admin)
        QSKIP("disabled");

    QVERIFY(captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("installed successfully"));

    QVERIFY(captureNpackdCLOutput("path -p io.mpv.mpv-64 -v 0.4").
            contains("mpv_64-bit"));
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

void App::addRemoveShare()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully");

    QVERIFY(captureNpackdCLOutput("add -p active-directory-explorer "
            "-f \"C:\\Program Files\\AD_addRemoveShare\"").
            contains("installed successfully"));

    captureOutput("C:\\Windows\\System32\\net.exe",
            "share \"AD_addRemoveShare=C:\\Program Files\\AD_addRemoveShare\"",
            "C:\\Windows");

    QProcess::execute("explorer.exe", QStringList("\\\\localhost\\AD_addRemoveShare"));

    QVERIFY(captureNpackdCLOutput("rm -p active-directory-explorer -d -e sd").
            contains("removed successfully"));

    QDir d;
    QVERIFY2(!d.exists("C:\\Program Files\\AD_addRemoveShare"),
            "C:\\Program Files\\AD_addRemoveShare");
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
            "installed successfully in \"C:\\Program Files\\ADE\""),
            output.toLatin1());

    output = captureNpackdCLOutput("rm -p active-directory-explorer");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::addToExistingDir()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p active-directory-explorer").
            contains("removed successfully");

    QString path = "C:\\Program Files\\AD_addToExistingDir";
    QDir d;
    d.mkpath(path);

    QString output = captureNpackdCLOutput(
            "add -p active-directory-explorer -f \"" + path + "\"");
    QVERIFY2(output.contains(
            "already exists"),
            output.toLatin1());
}

void App::updateToDir()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p jpegview");

    QString output = captureNpackdCLOutput(
            "add -p jpegview -v 1.0.36 -f \"C:\\Program Files\\JPEGView\"");
    QVERIFY2(output.contains(
            "installed successfully in \"C:\\Program Files\\JPEGView\""),
            output.toLatin1());

    output = captureNpackdCLOutput("update -p jpegview -f \"C:\\Program Files\\JPEGViewNew\"");
    QVERIFY2(output.contains(
            "The packages were updated successfully"),
            output.toLatin1());

    QVERIFY(captureNpackdCLOutput("path -p jpegview").
            trimmed() == "C:\\Program Files\\JPEGViewNew");

    output = captureNpackdCLOutput("rm -p jpegview");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::updateKeepDirectories()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p jpegview");

    QString output = captureNpackdCLOutput(
            "add -p jpegview -v 1.0.36 -f \"C:\\Program Files\\JPEGView\"");
    QVERIFY2(output.contains(
            "installed successfully in \"C:\\Program Files\\JPEGView\""),
            output.toLatin1());

    output = captureNpackdCLOutput("update -p jpegview -k");
    QVERIFY2(output.contains(
            "The packages were updated successfully"),
            output.toLatin1());

    QVERIFY(captureNpackdCLOutput("path -p jpegview").
            trimmed() == "C:\\Program Files\\JPEGView");

    output = captureNpackdCLOutput("rm -p jpegview");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::place()
{
    if (!admin)
        QSKIP("disabled");

    QTemporaryDir td;
    if (td.isValid()) {
        QString output = captureNpackdCLOutput(
                "place -p mytest -v 2.22 -f \"" + td.path() + "\"");
        QVERIFY2(output.contains("successfully"),  output.toLatin1());

        output = captureNpackdCLOutput(
                "place -p mytest2 -v 2.22 -f \"" + td.path() + "\"");
        QVERIFY2(output.contains("mytest 2.22"),  output.toLatin1());

        QVERIFY(captureNpackdCLOutput("path -p mytest").
                trimmed() == td.path().replace('/', '\\'));

        output = captureNpackdCLOutput("rm -p mytest");
        QVERIFY2(output.contains("removed successfully"), output.toLatin1());
    }
}

void App::addVersions()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("rm -p jpegview");

    QString output = captureNpackdCLOutput(
            "add -p jpegview -v 1.0.36 -f \"C:\\Program Files\\JPEGView\"");
    QVERIFY2(output.contains(
            "installed successfully in \"C:\\Program Files\\JPEGView\""),
            output.toLatin1());

    output = captureNpackdCLOutput("add -p jpegview -r [1,2)");
    QVERIFY2(output.contains(
            "installed successfully"),
            output.toLatin1());

    QVERIFY(captureNpackdCLOutput("path -p jpegview").
            trimmed() == "C:\\Program Files\\JPEGView");

    output = captureNpackdCLOutput("rm -p jpegview");
    QVERIFY2(output.contains("removed successfully"), output.toLatin1());
}

void App::updateInstall()
{
    if (!admin)
        QSKIP("disabled");

    captureNpackdCLOutput("set-install-dir -f \"C:\\Program Files\"");
    captureNpackdCLOutput("rm -p jpegview");

    QString output = captureNpackdCLOutput(
            "update -p jpegview");
    QVERIFY2(output.contains(
            "No installed version found for the package JPEGView"),
            output.toLatin1());

    output = captureNpackdCLOutput("update -p jpegview -i");
    QVERIFY2(output.contains(
            "The packages were updated successfully"),
            output.toLatin1());

    QVERIFY(captureNpackdCLOutput("path -p jpegview").
            trimmed() == "C:\\Program Files\\JPEGView");

    output = captureNpackdCLOutput("rm -p jpegview");
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
    QProcess::startDetached(dir + "\\ADExplorer.exe", QStringList());
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

    QVERIFY(captureNpackdCLOutput("").contains("command line interface"));
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

void App::installDir()
{
    QVERIFY(captureNpackdCLOutput(
            "set-install-dir -f \"C:\\Program Files (x86)\"").trimmed() == "");
    QVERIFY(captureNpackdCLOutput("install-dir").
            contains("C:\\Program Files (x86)"));

    QVERIFY(captureNpackdCLOutput("set-install-dir").trimmed() == "");
    QVERIFY(captureNpackdCLOutput("install-dir").
            contains("C:\\Program Files"));
}

void App::list()
{
    QVERIFY(captureNpackdCLOutput("list").contains("Windows Installer"));

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

void App::where()
{
    QVERIFY(captureNpackdCLOutput("where -f .Npackd\\Install.bat").
            contains("\\.Npackd\\Install.bat"));
}

void App::setRepo()
{
    QVERIFY(captureNpackdCLOutput("set-repo -u file:///C:/Users/t/projects/npackd-cpp/TestRepository2.xml").
            contains("repositories were changed successfully"));
    QVERIFY(captureNpackdCLOutput("list-repos").
            contains("file:///C:/Users/t/projects/npackd-cpp/TestRepository2.xml"));
}

void App::info()
{
    QVERIFY(captureNpackdCLOutput("info -p io.mpv.mpv").
            contains("command line movie player"));
}

void App::listRepos()
{
    QVERIFY(captureNpackdCLOutput("list-repos").
            contains("www.npackd.org"));
}

void App::check()
{
    QVERIFY(captureNpackdCLOutput("check").
            contains("All dependencies are installed"));
}


