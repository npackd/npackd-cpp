#include <QStringList>
#include <QList>
#include <QTemporaryFile>

#include <windows.h>
#include <msi.h>

#include "app.h"

#include "wpmutils.h"
#include "windowsregistry.h"
#include "controlpanelthirdpartypm.h"
#include "repository.h"

App::App()
{
}

int App::listMSI()
{
    QStringList sl = WPMUtils::findInstalledMSIProductNames();

    WPMUtils::writeln("Installed MSI Products");
    for (int i = 0; i < sl.count(); i++) {
        WPMUtils::writeln(sl.at(i));
    }

    return 0;
}

int App::process()
{
    cl.add("path", 'p',
            "directory path (e.g. C:\\Program Files (x86)\\MyProgram)",
            "path", false);
    cl.add("file", 'f',
            "path to an MSI package (e.g. C:\\Downloads\\MyProgram.msi)",
            "file", false);
    cl.add("timeout", 't',
            "timeout in milliseconds (e.g. 10000)",
            "duration", false);
    cl.add("title", 'c',
            "software title in the Software control panel)",
            "title", false);

    QString err = cl.parse();
    if (!err.isEmpty()) {
        WPMUtils::writeln("Error: " + err);
        return 1;
    }

    // cl.dump();

    int r = 0;

    QStringList fr = cl.getFreeArguments();

    if (fr.count() == 0) {
        help();
    } else if (fr.count() > 1) {
        WPMUtils::writeln("Unexpected argument: " + fr.at(1), false);
        r = 1;
    } else if (fr.at(0) == "help") {
        help();
    } else if (fr.at(0) == "add-path") {
        r = addPath();
    } else if (fr.at(0) == "remove-path") {
        r = removePath();
    } else if (fr.at(0) == "list-msi") {
        r = listMSI();
    } else if (fr.at(0) == "get-product-code") {
        r = getProductCode();
    } else if (fr.at(0) == "wait") {
        r = wait();
    } else if (fr.at(0) == "remove" || fr.at(0) == "rm") {
        r = remove();
    } else {
        WPMUtils::writeln("Wrong command: " + fr.at(0), false);
        r = 1;
    }

    return r;
}

int App::remove()
{
    Job job;

    QString title = cl.get("title");

    if (job.shouldProceed()) {
        if (title.isNull()) {
            job.setErrorMessage("Missing option: --title");
        }
    }

    Repository rep;
    QList<InstalledPackageVersion*> installed;
    if (job.shouldProceed()) {
        ControlPanelThirdPartyPM cppm;
        Job* scanJob = job.newSubJob(0.1, "Scanning for packages", true, true);
        cppm.scan(scanJob, &installed, &rep);
        delete scanJob;
    }

    Package* found = 0;
    if (job.shouldProceed()) {
        for (int i = 0; i < rep.packages.size(); i++) {
            Package* p = rep.packages.at(i);
            if (p->title.compare(title, Qt::CaseInsensitive) == 0) {
                found = p;
                break;
            }
        }

        if (!found)
            job.setErrorMessage("Cannot find the package");
    }

    PackageVersion* pv = 0;
    if (job.shouldProceed()) {
        for (int i = 0; i < installed.size(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            if (ipv->package == found->name && ipv->installed()) {
                QString err;
                pv = rep.findPackageVersion_(
                        ipv->package, ipv->version, &err);
                if (!err.isEmpty()) {
                    job.setErrorMessage(err);
                }
                break;
            }
        }
        if (!pv)
            job.setErrorMessage("Cannot find the package version");
    }

    PackageVersionFile* pvf = 0;
    if (job.shouldProceed()) {
        for (int j = 0; j < pv->files.size(); j++) {
            if (pv->files.at(j)->path.compare(
                    ".Npackd\\Uninstall.bat",
                    Qt::CaseInsensitive)) {
                pvf = pv->files.at(j);
            }
        }
        if (!pvf)
            job.setErrorMessage("Removal script was not found");
    }

    if (job.shouldProceed()) {
        QString cmd = pvf->content;
        QTemporaryFile of(QDir::tempPath() + "\\cluXXXXXX.bat");
        if (!of.open())
            job.setErrorMessage(of.errorString());
        else {
            of.setAutoRemove(false);
            QString path = of.fileName();
            QString where = QDir().absoluteFilePath(path);
            if (of.write("\xff\xfe") == -1)
                job.setErrorMessage(of.errorString());

            if (of.write((const char*) cmd.utf16(), cmd.length() * 2) == -1)
                job.setErrorMessage(of.errorString());

            of.close();
            Job* execJob = job.newSubJob(0.8, "Running the script", true, true);

            // TODO: Output.log
            WPMUtils::executeBatchFile(execJob, where,
                    path, "Output.log", QStringList(), true);
        }
    }

    if (job.getErrorMessage().isEmpty())
        return 0;
    else {
        WPMUtils::writeln(job.getErrorMessage(), false);
        return 1;
    }
}

int App::getProductCode()
{
    int ret = 0;

    QString file = cl.get("file");

    if (ret == 0) {
        if (file.isNull()) {
            WPMUtils::writeln("Missing option: --file", false);
            ret = 1;
        }
    }

    if (ret == 0) {
        MSIHANDLE hProduct;
        UINT r = MsiOpenPackageW((WCHAR*) file.utf16(), &hProduct);
        if (!r) {
            WCHAR guid[40];
            DWORD pcchValueBuf = 40;
            r = MsiGetProductPropertyW(hProduct, L"ProductCode", guid, &pcchValueBuf);
            if (!r) {
                QString s;
                s.setUtf16((ushort*) guid, pcchValueBuf);
                WPMUtils::writeln(s);
            } else {
                WPMUtils::writeln(
                        "Cannot get the value of the ProductCode property", false);
                ret = 1;
            }
        } else {
            WPMUtils::writeln("Cannot open the MSI file", false);
            ret = 1;
        }
    }

    return ret;
}

int App::wait()
{
    int ret = 0;

    QString timeout = cl.get("timeout");

    if (ret == 0) {
        if (timeout.isNull()) {
            WPMUtils::writeln("Missing option: --timeout", false);
            ret = 1;
        }
    }

    int t = 0;
    if (ret == 0) {
        bool ok;
        t = timeout.toInt(&ok);
        if (!ok) {
            WPMUtils::writeln(QString("Not a number: %1").arg(timeout), false);
            ret = 1;
        }
    }

    if (ret == 0) {
        Sleep(t);
    }

    return ret;
}

int App::help()
{
    const char* lines[] = {
        "CLU - Command line utility",
        "Usage:",
        "    clu [help]",
        "        prints this help",
        "    clu add-path --path=<path>",
        "        appends the specified path to the system-wide PATH variable",
        "    clu remove-path --path=<path>",
        "        removes the specified path from the system-wide PATH variable",
        "    clu list-msi",
        "        lists all installed MSI packages",
        "    clu get-product-code --file=<file>",
        "        prints the product code of an MSI file",
        "    clu wait --timeout=<milliseconds>",
        "        wait for the specified amount of time",
        "    clu remove|rm --title=<title>",
        "        removes the specified software from the Software control panel",
        "Options:",
    };
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        WPMUtils::writeln(QString(lines[i]));
    }
    this->cl.printOptions();
    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occcures.",
        "If the output is redirected, the texts will be encoded as UTF-8.",
    };
    for (int i = 0; i < (int) (sizeof(lines2) / sizeof(lines2[0])); i++) {
        WPMUtils::writeln(QString(lines2[i]));
    }

    return 0;
}

int App::addPath()
{
    int r = 0;

    QString path = cl.get("path");

    if (r == 0) {
        if (path.isNull()) {
            WPMUtils::writeln("Missing option: --path", false);
            r = 1;
        }
    }

    if (r == 0) {
        if (path.contains(';')) {
            WPMUtils::writeln("The path cannot contain a semicolon",
                    false);
            r = 1;
        }
    }

    if (r == 0) {
        QString mpath = path.toLower().trimmed();
        mpath.replace('/', '\\');

        QString err;
        QString curPath = WPMUtils::getSystemEnvVar("PATH", &err);
        if (err.isEmpty()) {
            QStringList sl = curPath.split(';');
            bool found = false;
            for (int i = 0; i < sl.count(); i++) {
                QString s = sl.at(i).toLower().trimmed();
                s.replace('/', '\\');
                if (s == mpath) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (!curPath.endsWith(';'))
                    curPath += ';';
                curPath += path;

                // it's actually 2048, but Explorer seems to have a bug
                if (curPath.length() < 2040) {
                    err = WPMUtils::setSystemEnvVar("PATH", curPath, true);
                    if (!err.isEmpty()) {
                        r = 1;
                        WPMUtils::writeln(err, false);
                    } else {
                        WPMUtils::fireEnvChanged();
                    }
                } else {
                    r = 1;
                    WPMUtils::writeln(
                            "The new PATH value would be too long", false);
                }
            }
        } else {
            r = 1;
            WPMUtils::writeln(err, false);
        }
    }

    return r;
}

int App::removePath()
{
    int r = 0;

    QString path = cl.get("path");

    if (r == 0) {
        if (path.isNull()) {
            WPMUtils::writeln("Missing option: --path", false);
            r = 1;
        }
    }

    if (r == 0) {
        if (path.contains(';')) {
            WPMUtils::writeln("The path cannot contain a semicolon",
                    false);
            r = 1;
        }
    }

    if (r == 0) {
        QString mpath = path.toLower().trimmed();
        mpath.replace('/', '\\');

        QString err;
        QString curPath = WPMUtils::getSystemEnvVar("PATH", &err);
        if (err.isEmpty()) {
            QStringList sl = curPath.split(';');
            int index = -1;
            for (int i = 0; i < sl.count(); i++) {
                QString s = sl.at(i).toLower().trimmed();
                s.replace('/', '\\');
                if (s == mpath) {
                    index = i;
                    break;
                }
            }
            if (index >= 0) {
                sl.removeAt(index);
                curPath = sl.join(";");

                // it's actually 2048, but Explorer seems to have a bug
                if (curPath.length() < 2040) {
                    err = WPMUtils::setSystemEnvVar("PATH", curPath, true);
                    if (!err.isEmpty()) {
                        r = 1;
                        WPMUtils::writeln(err, false);
                    } else {
                        WPMUtils::fireEnvChanged();
                    }
                } else {
                    r = 1;
                    WPMUtils::writeln(
                            "The new PATH value would be too long", false);
                }
            }
        } else {
            r = 1;
            WPMUtils::writeln(err, false);
        }
    }

    return r;
}
