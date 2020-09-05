#include <QStringList>
#include <QList>
#include <QTemporaryFile>
#include <QDirIterator>

#include <windows.h>
#include <msi.h>

#include "app.h"

#include "wpmutils.h"
#include "windowsregistry.h"
#include "controlpanelthirdpartypm.h"
#include "repository.h"
#include "packageutils.h"

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

void App::unwrapDir(Job *job)
{
    QString path = cl.get("path");

    if (job->shouldProceed()) {
        if (path.isNull()) {
            job->setErrorMessage("Missing option: --path");
        }
    }

    if (job->shouldProceed()) {
        if (path.contains('*') || path.contains('?')) {
            path = WPMUtils::normalizePath(path, false);
            QStringList nameFilters;
            int p = path.lastIndexOf('\\');
            if (p >= 0) {
                path = path.left(p);
                nameFilters.append(path.mid(p + 1));
            } else {
                nameFilters.append(path);
                path.clear();
            }

            path = QDir(path).absolutePath();

            QDirIterator it(path, nameFilters, QDir::Dirs);
            if (it.hasNext()) {
                path = it.next();
            } else {
                job->setErrorMessage(QString("No directory found for \"%0\" in \"%1\"").
                        arg(nameFilters.at(0)).arg(path));
            }
        }
    }

    if (job->shouldProceed()) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isDir()) {
            job->setErrorMessage(QString("Invalid directory: %0").arg(path));
        }
    }

    if (job->shouldProceed()) {
        QDir aDir(path);
        QFileInfoList entries = aDir.entryInfoList(
                QDir::NoDotAndDotDot |
                QDir::AllEntries | QDir::System);
        int count = entries.size();
        for (int idx = 0; idx < count; idx++) {
            QFileInfo entryInfo = entries[idx];

            // special case: a sub-directory/file with the same name as the parent directory
            if (entryInfo.fileName().compare(aDir.dirName(), Qt::CaseInsensitive) != 0) {
                aDir.rename(entryInfo.fileName(), QStringLiteral("..\\") + entryInfo.fileName());
            }
            job->setProgress(idx / static_cast<double>(count + 1));

            if (!job->shouldProceed())
                break;
        }

        // if an entry with the same name exists inside
        if (!aDir.dirName().isEmpty() && aDir.exists(aDir.dirName())) {
            Job* sub = job->newSubJob(1 / static_cast<double>(count + 1),
                    QString("Unwrap directory %0").arg(aDir.path() + "\\" + aDir.dirName()),
                    true, true);
            unwrapDir(sub);
        }

        if (job->shouldProceed()) {
            // if the directory was not deleted, it is not a problem
            Job* sub = job->newSubJob(1 / static_cast<double>(count + 1),
                    QString("Delete directory %0").arg(aDir.path()),
                    true, false);
            WPMUtils::removeDirectory(sub, aDir.absolutePath());
        }

        if (job->shouldProceed())
            job->setProgress(1);
    }

    job->complete();
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
            "software title in the Software control panel",
            "title", false);
    cl.add("encoding", 'e', "file encoding",
            "UTF-8 or UTF-16", false);

    QString err = cl.parse();
    if (!err.isEmpty()) {
        WPMUtils::writeln("Error: " + err);
        return 1;
    }

    // cl.dump();

    int r = 0;

    QList<CommandLine::ParsedOption*> options = cl.getParsedOptions();

    QString cmd;
    if (options.size() > 0 && options.at(0)->opt == nullptr) {
        cmd = options.at(0)->value;
    }

    if (options.count() == 0) {
        help();
    } else if (cmd.isEmpty() || cmd == "help") {
        help();
    } else if (cmd == "add-path") {
        r = addPath();
    } else if (cmd == "remove-path") {
        r = removePath();
    } else if (cmd == "list-msi") {
        r = listMSI();
    } else if (cmd == "get-product-code") {
        r = getProductCode();
    } else if (cmd == "wait") {
        r = wait();
//    } else if (fr.at(0) == "remove" || fr.at(0) == "rm") {
//        r = remove();
    } else if (cmd == "unwrap-dir") {
        Job* job = new Job();
        unwrapDir(job);
        if (!job->getErrorMessage().isEmpty()) {
            r = 1;
            WPMUtils::writeln(job->getErrorMessage() + "\n", false);
        }
        delete job;
    } else {
        WPMUtils::writeln("Wrong command: " + cmd, false);
        r = 1;
    }

    return r;
}

int App::remove()
{
    Job* job = clp.createJob();
    //clp.setUpdateRate(0);

    QString title = cl.get("title");

    if (job->shouldProceed()) {
        if (title.isNull()) {
            job->setErrorMessage("Missing option: --title");
        }
    }

    Repository rep;
    QList<InstalledPackageVersion*> installed;
    if (job->shouldProceed()) {
        ControlPanelThirdPartyPM cppm;
        Job* scanJob = job->newSubJob(0.1, "Scanning for packages", true, true);
        cppm.scan(scanJob, &installed, &rep);
    }

    Package* found = nullptr;
    if (job->shouldProceed()) {
        QRegExp re(title, Qt::CaseInsensitive);
        for (int i = 0; i < rep.packages.size(); i++) {
            Package* p = rep.packages.at(i);
            if (re.indexIn(p->title) >= 0) {
                found = p;
                break;
            }
        }

        if (!found)
            job->setErrorMessage("Cannot find the package");
    }

    PackageVersion* pv = nullptr;
    if (job->shouldProceed()) {
        for (int i = 0; i < installed.size(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            if (ipv->package == found->name && ipv->installed()) {
                QString err;
                pv = rep.findPackageVersion_(
                        ipv->package, ipv->version, &err);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                }
                break;
            }
        }
        if (job->shouldProceed() && !pv)
            job->setErrorMessage("Cannot find the package version");
    }

    PackageVersionFile* pvf = nullptr;
    if (job->shouldProceed()) {
        for (int j = 0; j < pv->files.size(); j++) {
            if (pv->files.at(j)->path.compare(
                    ".Npackd\\Uninstall.bat",
                    Qt::CaseInsensitive) == 0) {
                pvf = pv->files.at(j);
            }
        }
        if (job->shouldProceed() && !pvf)
            job->setErrorMessage("Removal script was not found");
    }

    if (job->shouldProceed()) {
        QString cmd = pvf->content;
        QTemporaryFile of(QDir::tempPath() + "\\cluXXXXXX.bat");
        if (!of.open())
            job->setErrorMessage(of.errorString());
        else {
            of.setAutoRemove(false);
            QString path = of.fileName();
            QDir dir(path);
            path = dir.dirName();
            dir.cdUp();
            QString where = dir.absolutePath();

            QByteArray ba = cmd.toUtf8();
            if (of.write(ba.data(), ba.length()) == -1)
                job->setErrorMessage(of.errorString());

            of.close();
            Job* execJob = job->newSubJob(0.8, "Running the script", true, true);

            where.replace('/', '\\');

            //WPMUtils::writeln(path + " " + where);

            WPMUtils::executeBatchFile(execJob, where,
                    path, "", QStringList(), false);
        }
    }

    QString err = job->getErrorMessage();
    delete job;

    if (err.isEmpty())
        return 0;
    else {
        WPMUtils::writeln(err, false);
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
        UINT r = MsiOpenPackageW(WPMUtils::toLPWSTR(file), &hProduct);
        if (!r) {
            WCHAR guid[40];
            DWORD pcchValueBuf = 40;
            r = MsiGetProductPropertyW(hProduct, L"ProductCode", guid, &pcchValueBuf);
            if (!r) {
                QString s = QString::fromUtf16(reinterpret_cast<ushort*>(guid));
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
        Sleep(static_cast<DWORD>(t));
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
        "    clu unwrap-dir --path <path>",
        "        move the content of a directory one level higher.",
        "        The last part of the path may contain * or ?.",
        "    clu wait --timeout=<milliseconds>",
        "        wait for the specified amount of time",
//        "    clu remove|rm --title=<regular expression>",
//        "        removes one installed program from the Software control panel",
        "Options:",
    };
    for (int i = 0; i < static_cast<int>(sizeof(lines) / sizeof(lines[0])); i++) {
        WPMUtils::writeln(QString(lines[i]));
    }
    QStringList txt = this->cl.printOptions();
    for (int i = 0; i < txt.count(); i++) {
        WPMUtils::writeln(txt[i]);
    }
    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occcures.",
        "If the output is redirected, the texts will be encoded as UTF-8.",
    };
    for (int i = 0; i < static_cast<int>(sizeof(lines2) / sizeof(lines2[0])); i++) {
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
                    err = WPMUtils::setSystemEnvVar("PATH", curPath, true, true);
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
                    err = WPMUtils::setSystemEnvVar("PATH", curPath, true, true);
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
