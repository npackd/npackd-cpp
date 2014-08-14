#include <QStringList>
#include <QCoreApplication>

#include "app.h"
#include "job.h"
#include "packageversion.h"
#include "wpmutils.h"
#include "clprogress.h"

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

int App::functionalTest()
{
    int r = 0;

    WPMUtils::outputTextConsole("---- testing add\n");
    if (!captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("installed successfully")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing add twice\n");
    if (!captureNpackdCLOutput("add -p io.mpv.mpv-64 -v 0.4").
            contains("is already installed in")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing rm\n");
    if (!captureNpackdCLOutput("rm -p io.mpv.mpv-64 -v 0.4").
            contains("removed successfully")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing help\n");
    if (!captureNpackdCLOutput("help").contains("ncl update")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing missing command\n");
    if (!captureNpackdCLOutput("").contains("Missing command")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing list\n");
    if (!captureNpackdCLOutput("list").contains("Windows Installer 5.0")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing list\n");
    if (captureNpackdCLOutput("list --bare-format").
            contains("Reading list of installed packages")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing detect\n");
    if (!captureNpackdCLOutput("detect").
            contains("Package detection completed successfully")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing add-repo\n");
    if (!captureNpackdCLOutput("add-repo --url http://localhost:8083/NonExisting.xml").
            contains("The repository was added successfully")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing remove-repo\n");
    if (!captureNpackdCLOutput("remove-repo --url http://localhost:8083/NonExisting.xml").
            contains("The repository was removed successfully")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing search\n");
    if (!captureNpackdCLOutput("search -q \"c++\"").
            contains("Microsoft Visual C++")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing search -b\n");
    if (captureNpackdCLOutput("search -q \"c++\" -b").
            contains("packages found")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing info\n");
    if (!captureNpackdCLOutput("info -p io.mpv.mpv").
            contains("command line movie player")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing list-repos\n");
    if (!captureNpackdCLOutput("list-repos").
            contains("npackd.appspot.com")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole("---- testing check\n");
    if (!captureNpackdCLOutput("check").
            contains("All dependencies are installed")) {
        WPMUtils::outputTextConsole("---- test failed\n", false);
        r++;
    } else {
        WPMUtils::outputTextConsole("---- test succeeded\n");
    }

    WPMUtils::outputTextConsole(QString("---- %1 tests failed\n").arg(r));

    QCoreApplication::instance()->exit(r);

    return r;
}
