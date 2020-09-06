#include <limits>
#include <math.h>

#include <QMultiMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

#include "app.h"
#include "wpmutils.h"
#include "commandline.h"
#include "downloader.h"
#include "installedpackages.h"
#include "installedpackageversion.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "hrtimer.h"
#include "controlpanelthirdpartypm.h"
#include "packageutils.h"

static bool compareByPackageTitle(const QPair<PackageVersion*, QString>& e1,
        const QPair<PackageVersion*, QString>& e2) {
    PackageVersion* pv1 = e1.first;
    PackageVersion* pv2 = e2.first;

    if (pv1->package == pv2->package)
        return pv1->version.compare(pv2->version) < 0;
    else {
        QString pt1 = e1.second;
        QString pt2 = e2.second;
        return pt1.toLower() < pt2.toLower();
    }
}

bool packageLessThan(const Package* p1, const Package* p2)
{
    return p1->title.toLower() < p2->title.toLower();
}

App::App() : currentJob(nullptr)
{

}

QStringList App::sortPackageVersionsByPackageTitle(
        QList<PackageVersion*> *list) {
    QList<QPair<PackageVersion*, QString> > items;

    for (int i = 0; i < list->count(); i++) {
        PackageVersion* pv = list->at(i);
        QString s = pv->getPackageTitle();

        QPair<PackageVersion*, QString> pair;
        pair.first = pv;
        pair.second = s;
        items.append(pair);
    }

    std::sort(items.begin(), items.end(), compareByPackageTitle);

    QStringList titles;
    for (int i = 0; i < list->count(); i++) {
        (*list)[i] = items.at(i).first;
        titles.append(items.at(i).second);
    }

    return titles;
}

int App::process()
{
    // alphabetically sorted options by the short name
    cl.add("bare-format", 'b', "bare format (no heading or summary)",
            "", false, "list,list-repos,search,install-dir,which,where,info,path");
    cl.add("cmd", 'c', "output a .cmd script",
            "", false, "path");
    cl.add("debug", 'd', "turn on the debug output", "", false);
    cl.add("end-process", 'e',
            "list of ways to close running applications \r\n(c=close, k=kill, s=disconnect from file shares, d=stop services, t=send Ctrl+C). The default value is 'c'.",
            "[c][k][s][t]", false, "remove,rm,update");
    cl.add("file", 'f', "file or directory", "file", false,
            "add,place,set-install-dir,update,where,which,path");
    cl.add("install", 'i',
            "install a package if it was not installed", "", false, "update");
    cl.add("json", 'j', "json format for the output",
            "", false, "list,list-repos,search,install-dir,which,where,info,path");
    cl.add("keep-directories", 'k',
            "use the same directories for updated packages", "", false,
           "update");
    cl.add("local", 'l',
            "install packages for the current user instead of system-wide",
            "", false);
    cl.add("non-interactive", 'n',
            "assume that there is no user and do not ask for input", "", false);
    cl.add("package", 'p',
            "internal package name (e.g. com.example.Editor or just Editor)",
            "package", true, "add,info,path,place,remove,update,rm,build");
    cl.add("query", 'q', "search terms (e.g. editor)",
            "search terms", false, "search,update");
    cl.add("versions", 'r', "versions range (e.g. [1.5,2))",
            "range", true, "add,path,update");

    // --status for the command "list" is supported for compatibility reasons
    cl.add("status", 's', "filters packages by status",
            "status", false, "list,search");

    cl.add("timeout", 't', "timeout in seconds",
            "seconds", false, "remove,rm,update,add");
    cl.add("url", 'u', "repository URL (e.g. https://www.example.com/Rep.xml)",
            "repository", false, "add-repo,remove-repo,set-repo,add,update,search");
    cl.add("version", 'v', "version number (e.g. 1.5.12)",
            "version", false, "add,info,path,place,rm,remove");

    cl.add("user", 0, "user name for the HTTP authentication",
            "user name", false, "add,update,detect,search");
    cl.add("password", 0, "password for the HTTP authentication",
            "password", false, "add,update,detect,search");

    cl.add("proxy-user", 0, "user name for the HTTP proxy authentication",
            "user name", false, "add,update,detect,search");
    cl.add("proxy-password", 0, "password for the HTTP proxy authentication",
            "password", false, "add,update,detect,search");

    cl.add("title", 0, "package title or a regular expression in JavaScript syntax. Example: /PDF/i",
            "title", false, "remove-scp");

    cl.add("output-package", 0,
            "internal package name (e.g. com.example.Editor or just Editor)",
            "package", true, "build");

    QString err = cl.parse();
    if (!err.isEmpty()) {
        err = "Error: " + err;
    }

    // cl.dump();

    if (err.isEmpty()) {
        this->interactive = !cl.isPresent("non-interactive");

        this->debug = cl.isPresent("debug");

        if (cl.isPresent("local"))
            PackageUtils::globalMode = false;

        if (debug) {
            clp.setUpdateRate(0);

            QLoggingCategory::setFilterRules("npackd=true");
        }
    }

    QList<CommandLine::ParsedOption*> options = cl.getParsedOptions();

    QString cmd;
    if (options.size() > 0 && options.at(0)->opt == nullptr) {
        cmd = options.at(0)->value;
    }

    if (!err.isEmpty()) {
        // nothing. The error will be processed later.
    } else if (options.size() == 0) {
        Job* job = new Job();
        usage(job);
        delete job;
    } else if (cmd.isEmpty()) {
        err = QStringLiteral("Missing command");
    } else {
        QList<CommandLine::ParsedOption*> parsed = cl.getParsedOptions();
        for (int i = 0; i < parsed.count(); i++) {
            CommandLine::Option* opt = parsed.at(i)->opt;
            if (opt && opt->allowedCommands.count() > 0) {
                // qCDebug(npackd) << "1" << opt->allowedCommands.count();
                if (!opt->allowedCommands.contains(cmd)) {
                    err = "The option --" + opt->name +
                            " is not allowed for the command \"" + cmd + "\". Allowed commands: " +
                            opt->allowedCommands.join(',');
                    break;
                }
            }
        }

        Job* job;
        if (cl.isPresent("bare-format") || cl.isPresent("json") ||
                cmd == "help" || cmd == "path")
            job = new Job();
        else
            job = clp.createJob();

        this->currentJob = job;

        QString timeout = cl.get("timeout");
        if (!timeout.isNull()) {
            bool ok;
            int timeout_ = timeout.toInt(&ok);
            if (ok) {
                if (timeout_ > 0)
                    job->setTimeout(timeout_);
                else
                    err = "The value for --timeout should be positive";
            } else {
                err = "The value for --timeout is not a valid number";
            }
        }

        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else if (cmd == "help") {
            usage(job);
        } else if (cmd == "path") {
            path(job);
        } else if (cmd == "place") {
            place(job);
        } else if (cmd == "remove" || cmd == "rm") {
            remove(job);
        } else if (cmd == "add") {
            add(job);
        } else if (cmd == "add-repo") {
            addRepo(job);
        } else if (cmd == "set-repo") {
            setRepo(job);
        } else if (cmd == "remove-repo") {
            removeRepo(job);
        } else if (cmd == "remove-scp") {
            removeSCP(job);
        } else if (cmd == "list-repos") {
            listRepos(job);
        } else if (cmd == "search") {
            search(job);
        } else if (cmd == "check") {
            check(job);
        } else if (cmd == "which") {
            which(job);
        } else if (cmd == "where") {
            where(job);
        } else if (cmd == "list") {
            list(job);
        } else if (cmd == "info") {
            info(job);
        } else if (cmd == "update") {
            update(job);
        } else if (cmd == "detect") {
            detect(job);
        } else if (cmd == "set-install-dir") {
            setInstallPath(job);
        } else if (cmd == "install-dir") {
            getInstallPath(job);
        } else if (cmd == "build") {
            build(job);
        } else {
            job->setErrorMessage(QStringLiteral("Wrong command: ") + cmd +
                    QStringLiteral(". Try \"ncl help\""));
        }

        err = job->getErrorMessage();

        this->currentJob = nullptr;

        delete job;
    }

    int r = 0;
    if (err.isEmpty())
        r = 0;
    else {
        r = 1;
        qCCritical(npackd).noquote() << err;
    }

    QCoreApplication::instance()->exit(r);

    return r;
}

void App::printJSON(const QJsonObject &obj)
{
    // WPMUtils::writeln does not work for very long texts

    QJsonDocument d(obj);

    QStringList sl = QString::fromUtf8(d.toJson(
            QJsonDocument::Indented)).split("\n");
    for (int i = 0; i < sl.count(); i++) {
        if (i != sl.count() - 1)
            WPMUtils::writeln(sl.at(i));
        else
            WPMUtils::outputTextConsole(sl.at(i));
    }
}

QString App::addNpackdCL(DBRepository* r)
{
    QString err;

    Version myVersion;
    (void) myVersion.setVersion(NPACKD_VERSION);
    PackageVersion* pv = r->findPackageVersion_(
            "com.googlecode.windows-package-manager.NpackdCL",
            myVersion, &err);
    if (!pv) {
        pv = new PackageVersion(
                "com.googlecode.windows-package-manager.NpackdCL");
        pv->version = myVersion;
        r->savePackageVersion(pv, true);
    }
    delete pv;
    pv = nullptr;

    err = InstalledPackages::getDefault()->updateNpackdCLEnvVar();

    return err;
}

void App::usage(Job* job)
{
    Version my;
    my.setVersion(NPACKD_VERSION);
    my.normalize();

    WPMUtils::writeln(QString(
            "ncl %1 - command line interface for the Npackd software package manager").
            arg(my.getVersionString()));
    const char* lines[] = {
        "Usage: ncl <command> [global options] [options]",
        "",
        "Available commands in alphabetical order:",
        "    ncl add (--package <package>",
        "            [--version <version> | --versions <versions>])+ ",
        "            [--file <installation directory>]",
        "            [--user <user name>] [--password <password>]",
        "            [--proxy-user <proxy user name>] [--proxy-password <proxy password>]",
        "            (--url <repository>)*",
        "        installs packages. The newest available version will be ",
        "        installed, if none is specified.",
        "    ncl add-repo --url <repository>",
        "        appends a repository to the system-wide list of package sources",
        "    ncl build --package <package> [--version <version> | --versions <versions>])",
        "            --output-package <package>",
        "        build a package from another one (e.g. a binary from source code)",
        "    ncl check",
        "        checks the installed packages for missing dependencies",
        "    ncl detect [--user <user name>] [--password <password>]",
        "            [--proxy-user <proxy user name>] [--proxy-password <proxy password>]",
        "        download repositories and detect packages from the MSI ",
        "        database and software control panel",
        "    ncl info --package <package> [--version <version>]",
        "            [--bare-format | --json]",
        "        shows information about the specified package or package version",
        "    ncl install-dir [--bare-format | --json]",
        "        prints the directory where packages will be installed",
        "    ncl list [--bare-format | --json]",
        "        lists package versions sorted by package name and version.",
        "    ncl list-repos [--bare-format | --json]",
        "        prints the system-wide list of package sources (repositories)",
        "    ncl help",
        "        prints this help",
        "    ncl path (--package <package>",
        "            [--version <version> | --versions <versions>])+ ",
        "            [--cmd | --json]",
        "        searches for installed packages and prints their locations",
        "    ncl place --package <package>",
        "            --version <version> --file <directory>",
        "            [--bare-format | --json]",
        "        registers a package version installed without Npackd",
        "    ncl remove|rm (--package <package> [--version <version>])+",
        "           [--end-process <types>]",
        "        removes packages. The version number may be omitted, ",
        "        if only one is installed.",
        "    ncl remove-scp --title <title>",
        "        remove a program for the Software Control Panel by title",
        "    ncl remove-repo --url <repository>",
        "        removes a repository from the system-wide list of package sources",
        "    ncl set-repo (--url <repository>)+",
        "        changes the system-wide list of package sources (repositories)",
        "    ncl search [--query <search terms>] ",
        "            [--status installed | updateable | all]",
        "            [--bare-format | --json]",
        "            (--url <repository>)*",
        "        full text search. Lists found packages sorted by package name.",
        "        All packages are shown by default.",
        "    ncl set-install-dir [--file <directory>]",
        "        changes the directory where packages will be installed. The",
        "        default directory for program files is used if the --file",
        "        parameter is missing.",
        "    ncl update (--package <package> [--versions <versions>])+",
        "            [--query <search terms>]",
        "            [--end-process <types>]",
        "            [--install] [--keep-directories]",
        "            [--file <installation directory>]",
        "            [--user <user name>] [--password <password>]",
        "            [--proxy-user <proxy user name>] [--proxy-password <proxy password>]",
        "            (--url <repository>)*",
        "        updates packages by uninstalling the currently installed",
        "        and installing the newest version. ",
        "    ncl where --file <relative path> [--bare-format | --json]",
        "        finds all installed packages with the specified file or directory",
        "    ncl which --file <file> [--bare-format | --json]",
        "        finds the package that owns the specified file or directory",
    };
    for (int i = 0; i < static_cast<int>(sizeof(lines) / sizeof(lines[0])); i++) {
        WPMUtils::writeln(QString(lines[i]));
    }

    QStringList opts = this->cl.printOptions();
    for (int i = 0; i < opts.count(); i++) {
        WPMUtils::writeln(opts.at(i));
    }

    const char* lines2[] = {
        "",
        "You can use short package names, e.g. App instead of com.example.App.",
        "The process exits with the code unequal to 0 if an error occures.",
        "If the output is redirected, the texts will be encoded as UTF-8.",
        "",
        "See https://github.com/tim-lebedkov/npackd/wiki/CommandLine for more details.",
    };
    for (int i = 0; i < static_cast<int>(sizeof(lines2) / sizeof(lines2[0])); i++) {
        WPMUtils::writeln(QString(lines2[i]));
    }

    job->completeWithProgress();
}

void App::listRepos(Job* job)
{
    bool bare = cl.isPresent("bare-format");
    bool json = cl.isPresent("json");

    QString err;

    QList<QUrl*> urls = PackageUtils::getRepositoryURLs(&err);
    if (err.isEmpty()) {
        if (json) {
            QJsonObject top;
            QJsonArray repos;
            for (int i = 0; i < urls.size(); i++) {
                repos.append(urls.at(i)->toString(QUrl::FullyEncoded));
            }
            top["repositories"] = repos;

            printJSON(top);
        } else {
            if (!bare) {
                WPMUtils::writeln(QString("%1 repositories are defined:").
                        arg(urls.size()));
                WPMUtils::writeln("");
            }
            for (int i = 0; i < urls.size(); i++) {
                WPMUtils::writeln(urls.at(i)->toString(QUrl::FullyEncoded));
            }
        }
    } else {
        job->setErrorMessage(err);
    }
    qDeleteAll(urls);

    job->complete();
}

void App::getInstallPath(Job* job)
{
    bool json = cl.isPresent("json");

    if (json) {
        QJsonObject top;
        top["installDir"] = PackageUtils::getInstallationDirectory();
        printJSON(top);
    } else {
        WPMUtils::outputTextConsole(PackageUtils::getInstallationDirectory());
    }

    job->complete();
}

void App::which(Job* job)
{
    bool bare = cl.isPresent("bare-format");
    bool json = cl.isPresent("json");

    InstalledPackages* ip = InstalledPackages::getDefault();
    if (job->shouldProceed()) {
        QString r = ip->readRegistryDatabase();
        if (!r.isEmpty())
            job->setErrorMessage(r);
    }

    if (job->shouldProceed()) {
        QString r = DBRepository::getDefault()->openDefault("default", true);
        if (!r.isEmpty())
            job->setErrorMessage(r);
    }

    QString file = cl.get("file");
    if (job->shouldProceed()) {
        if (file.isNull()) {
            job->setErrorMessage("Missing option: --file");
        }
    }

    if (job->shouldProceed()) {
        QFileInfo fi(file);
        InstalledPackageVersion* f = ip->findOwner(fi.absoluteFilePath());
        if (f) {
            DBRepository* rep = DBRepository::getDefault();
            Package* p = rep->findPackage_(f->package);
            QString title = p ? p->title : "?";

            if (json) {
                QJsonObject top;
                QJsonObject v;
                v["package"] = f->package;
                v["name"] = f->version.getVersionString();
                v["path"] = f->directory;
                top["installed"] = v;
                printJSON(top);
            } else if (bare) {
                WPMUtils::writeln(QString(
                        "%1\t%2\t%3\t%4").
                        arg(title).arg(f->version.getVersionString()).
                        arg(f->package).arg(f->directory));
            } else {
                WPMUtils::writeln(QString(
                        "%1 %2 (%3) is installed in \"%4\"").
                        arg(title).arg(f->version.getVersionString()).
                        arg(f->package).arg(f->directory));
            }

            delete p;
            delete f;
        } else {
            if (json)
                printJSON(QJsonObject());
            else if (bare)
                ; // nothing
            else
                WPMUtils::writeln(QString("No package found for \"%1\"").
                        arg(file));
        }
    }

    job->complete();
}

void App::where(Job* job)
{
    InstalledPackages* ip = InstalledPackages::getDefault();
    if (job->shouldProceed()) {
        QString r = ip->readRegistryDatabase();
        if (!r.isEmpty())
            job->setErrorMessage(r);
    }

    QString file = cl.get("file");
    if (job->shouldProceed()) {
        if (file.isNull()) {
            job->setErrorMessage("Missing option: --file");
        }
    }

    if (job->shouldProceed()) {
        bool json = cl.isPresent("json");

        QStringList paths = ip->getAllInstalledPackagePaths();

        if (json) {
            QJsonObject top;
            QJsonArray a;
            for (int i = 0; i < paths.count(); i++) {
                QFileInfo fi(paths[i], file);
                if (fi.exists())
                    a.append(paths[i] + "\\" + file);
            }
            top["paths"] = a;
            printJSON(top);
        } else {
            for (int i = 0; i < paths.count(); i++) {
                QFileInfo fi(paths[i], file);
                if (fi.exists())
                    WPMUtils::writeln(paths[i] + "\\" + file);
            }
        }
    }

    job->complete();
}

void App::setInstallPath(Job* job)
{
    QString file = cl.get("file");
    if (job->shouldProceed()) {
        if (file.isNull()) {
            file = WPMUtils::getProgramFilesDir();
        }
    }

    if (job->shouldProceed()) {
        QFileInfo fi(file);
        file = fi.absoluteFilePath();
        file = WPMUtils::normalizePath(file, false);
    }

    if (job->shouldProceed()) {
        QString r = DBRepository::getDefault()->checkInstallationDirectory(file);
        if (!r.isEmpty())
            job->setErrorMessage(r);
    }

    if (job->shouldProceed()) {
        QString r = PackageUtils::setInstallationDirectory(file);
        if (!r.isEmpty())
            job->setErrorMessage(r);
    }

    // intentionally no success output here
    /*
    if (job->shouldProceed()) {
        qCInfo(npackdImportant()).noquote() << QString(
                "The installation path was changed successfully to \"%1\"").arg(file);
    }
    */

    job->complete();
}

void App::check(Job* job)
{
    job->setTitle("Checking dependency integrity for the installed packages");

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->openDefault();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                "Reading list of installed packages from the registry");
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.5,
                "Refreshing the list of installed packages");

        // ignoring the error message here as "check" should be available
        // for non-admins too
        InstalledPackages* def = InstalledPackages::getDefault();
        InstalledPackages ip;
        ip.refresh(DBRepository::getDefault(), sub);
        if (sub->shouldProceed()) {
            *def = ip;
            def->save();
        }
    }

    DBRepository* rep = DBRepository::getDefault();
    InstalledPackages* ip = InstalledPackages::getDefault();
    QList<PackageVersion*> list;

    if (job->shouldProceed()) {
        QString err;
        list = rep->getInstalled_(&err);
        if (err.isEmpty())
            job->setProgress(0.9);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        sortPackageVersionsByPackageTitle(&list);

        job->setProgress(1);

        int n = 0;
        for (int i = 0; i < list.count(); i++) {
            PackageVersion* pv = list.at(i);
            for (int j = 0; j < pv->dependencies.count(); j++) {
                Dependency* d = pv->dependencies.at(j);
                if (!ip->isInstalled(*d)) {
                    WPMUtils::writeln(QString(
                            "%1 depends on %2, which is not installed").
                            arg(pv->toString(true)).
                            arg(rep->toString(*d, true)));
                    n++;
                }
            }
        }

        if (n == 0) {
            qCInfo(npackdImportant()).noquote() << "All dependencies are installed";
        }
    }

    qDeleteAll(list);

    job->complete();
}

void App::addRepo(Job* job)
{
    QString url = cl.get("url").trimmed();

    if (job->shouldProceed()) {
        if (url.isNull()) {
            job->setErrorMessage("Missing option: --url");
        }
    }

    QUrl* url_ = nullptr;
    if (job->shouldProceed()) {
        url_ = new QUrl();
        url_->setUrl(url, QUrl::TolerantMode);
        if (!url_->isValid()) {
            job->setErrorMessage("Invalid URL: " + url);
        }
    }

    if (job->shouldProceed()) {
        QString err;
        QList<QUrl*> urls = PackageUtils::getRepositoryURLs(&err);
        if (err.isEmpty()) {
            int found = -1;
            for (int i = 0; i < urls.size(); i++) {
                if (urls.at(i)->toString() == url_->toString()) {
                    found = i;
                    break;
                }
            }
            if (found >= 0) {
                WPMUtils::writeln(
                        "This repository is already registered: " + url);
            } else {
                urls.append(url_);
                url_ = nullptr;
                PackageUtils::setRepositoryURLs(urls, &err);
                if (err.isEmpty())
                    qCInfo(npackdImportant()).noquote() <<
                            "The repository was added successfully. Run \"ncl detect\" to update the local database.";
                else
                    job->setErrorMessage(err);
            }
        } else {
            job->setErrorMessage(err);
        }
        qDeleteAll(urls);
    }

    delete url_;

    job->complete();
}

void App::setRepo(Job* job)
{
    QStringList urls_ = cl.getAll("url");

    if (job->shouldProceed()) {
        if (urls_.count() == 0) {
            job->setErrorMessage("Missing option: --url");
        }
    }

    if (job->shouldProceed()) {
        QList<QUrl*> urls;
        for (int i = 0; i < urls_.count(); i++) {
            if (!job->shouldProceed())
                break;

            QString url = urls_.at(i);
            QUrl* url_ = new QUrl();
            url_->setUrl(url, QUrl::TolerantMode);
            if (!url_->isValid()) {
                job->setErrorMessage("Invalid URL: " + url);
            } else {
                urls.append(url_);
            }
        }

        if (job->shouldProceed()) {
            QString err;
            PackageUtils::setRepositoryURLs(urls, &err);
            if (err.isEmpty())
                qCInfo(npackdImportant()).noquote() <<
                        "The repositories were changed successfully. Run \"ncl detect\" to update the local database.";
            else
                job->setErrorMessage(err);
        }

        qDeleteAll(urls);
    }

    job->complete();
}

void App::list(Job* job)
{
    bool bare = cl.isPresent("bare-format");
    bool json = cl.isPresent("json");

    job->setTitle("Listing package versions");

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->openDefault("default", true);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                "Reading list of installed packages from the registry");
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    QList<PackageVersion*> list;
    QStringList titles;

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.99,
                "Getting the list of installed packages from the registry");
        DBRepository* rep = DBRepository::getDefault();

        QString err;
        list = rep->getInstalled_(&err);
        if (err.isEmpty()) {
            titles = sortPackageVersionsByPackageTitle(&list);
            sub->completeWithProgress();
            job->setProgress(1);
        } else {
            job->setErrorMessage(err);
        }
    }

    if (job->shouldProceed()) {
        if (json) {
            QJsonObject top;
            QJsonArray pvs;
            for (int i = 0; i < list.count(); i++) {
                QJsonObject pv_;
                PackageVersion* pv = list.at(i);
                pv->toJSON(pv_);
                pvs.append(pv_);
            }
            top["versions"] = pvs;

            printJSON(top);
        } else {
            if (!bare)
                WPMUtils::writeln(QString("%1 package versions found:\r\n").
                        arg(list.count()));

            for (int i = 0; i < list.count(); i++) {
                PackageVersion* pv = list.at(i);
                if (!bare)
                    WPMUtils::writeln(pv->toString() +
                            " (" + pv->package + ")");
                else
                    WPMUtils::writeln(pv->package + " " +
                            pv->version.getVersionString() + " " +
                            titles.at(i));
            }
        }
    }

    qDeleteAll(list);

    job->complete();
}

void App::search(Job* job)
{
    bool bare = cl.isPresent("bare-format");
    bool json = cl.isPresent("json");
    QStringList urls_ = cl.getAll("url");
    QString user = cl.get("user");
    QString password = cl.get("password");
    QString proxyUser = cl.get("proxy-user");
    QString proxyPassword = cl.get("proxy-password");
    QString query = cl.get("query");

    job->setTitle("Searching for packages");

    Package::Status minStatus = Package::INSTALLED;
    Package::Status maxStatus = Package::INSTALLED;
    if (job->shouldProceed()) {
        QString status = cl.get("status");
        if (!status.isNull()) {
            if (status == "all") {
                minStatus = Package::INSTALLED;
                maxStatus = Package::INSTALLED;
            } else if (status == "installed") {
                minStatus = Package::INSTALLED;
                maxStatus = Package::UPDATEABLE;
            } else if (status == "updateable") {
                minStatus = Package::UPDATEABLE;
                maxStatus = Package::NOT_INSTALLED_NOT_AVAILABLE;
            } else {
                job->setErrorMessage("Wrong status: " + status);
            }
        }
    }

    DBRepository* dbr = DBRepository::getDefault();
    QTemporaryFile tempFile;

    if (job->shouldProceed()) {
        if (urls_.count() == 0) {
            QString err = dbr->openDefault();
            if (!err.isEmpty())
                job->setErrorMessage(err);
            else
                job->setProgress(0.1);
        } else {
            QList<QUrl*> urls;

            for (int i = 0; i < urls_.count(); i++) {
                if (!job->shouldProceed())
                    break;

                QString url = urls_.at(i);
                QUrl* url_ = new QUrl();
                url_->setUrl(url, QUrl::TolerantMode);
                if (!url_->isValid()) {
                    job->setErrorMessage("Invalid URL: " + url);
                } else {
                    urls.append(url_);
                }
            }

            if (job->shouldProceed()) {
                if (!tempFile.open()) {
                    job->setErrorMessage(QObject::tr("Error creating a temporary file"));
                } else {
                    tempFile.close();
                }
            }

            if (job->shouldProceed()) {
                QString err = dbr->open(QStringLiteral("tempdb"),
                        tempFile.fileName());
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                }
            }

            InstalledPackages::getDefault()->readRegistryDatabase();

            if (job->shouldProceed()) {
                Job* sub = job->newSubJob(0.10,
                        QObject::tr("Updating the temporary database"), true, true);
                dbr->clearAndDownloadRepositories(sub, urls, interactive, user, password, proxyUser, proxyPassword, true, false);
            }

            qDeleteAll(urls);
            urls.clear();
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.96,
                "Reading list of installed packages from the registry");
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    QStringList packageNames;
    QList<Package*> list;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01, "Searching for packages");
        QString err;
        packageNames = dbr->findPackages(minStatus, maxStatus,
                query, -1, -1, &err);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01, "Fetching packages");
        QString err;
        list = dbr->findPackages(packageNames);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        std::sort(list.begin(), list.end(), packageLessThan);

        if (json) {
            QJsonObject top;

            QJsonArray ps;
            for (int i = 0; i < list.count(); i++) {
                Package* p = list.at(i);
                QJsonObject p_;
                p->toJSON(p_);
                ps.append(p_);
            }
            top["packages"] = ps;

            printJSON(top);
        } else {
            if (!bare)
                WPMUtils::writeln(QString("%1 packages found:\r\n").
                        arg(list.count()));

            for (int i = 0; i < list.count(); i++) {
                Package* p = list.at(i);
                if (!bare)
                    WPMUtils::writeln(p->title +
                            " (" + p->name + ")");
                else
                    WPMUtils::writeln(p->name + " " +
                            p->title);
            }
        }

        qDeleteAll(list);
        job->setProgress(1);
    }

    job->complete();
}

void App::removeRepo(Job* job)
{
    QString url = cl.get("url");

    if (job->shouldProceed()) {
        if (url.isNull()) {
            job->setErrorMessage("Missing option: --url");
        }
    }

    QUrl* url_ = nullptr;
    if (job->shouldProceed()) {
        url_ = new QUrl();
        url_->setUrl(url, QUrl::TolerantMode);
        if (!url_->isValid()) {
            job->setErrorMessage("Invalid URL: " + url);
        }
    }

    if (job->shouldProceed()) {
        QString err;
        QList<QUrl*> urls = PackageUtils::getRepositoryURLs(&err);
        if (err.isEmpty()) {
            int found = -1;
            for (int i = 0; i < urls.size(); i++) {
                if (urls.at(i)->toString() == url_->toString()) {
                    found = i;
                    break;
                }
            }
            if (found < 0) {
                WPMUtils::writeln(
                        "The repository was not in the list: " +
                        url);
            } else {
                delete urls.takeAt(found);
                PackageUtils::setRepositoryURLs(urls, &err);
                if (err.isEmpty())
                    qCInfo(npackdImportant()).noquote() <<
                            "The repository was removed successfully. Run \"ncl detect\" to update the local database.";
                else
                    job->setErrorMessage(err);
            }
        } else {
            job->setErrorMessage(err);
        }
        qDeleteAll(urls);
    }

    delete url_;

    job->complete();
}

#if defined(max) && defined(_MSC_VER)
#undef max
#endif

void App::path(Job* job)
{
    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->openDefault();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1,
                "Reading list of installed packages from the registry");
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    QString err;
    QList<InstalledPackageVersion*> ipvs =
            PackageVersion::getPathPackageVersionOptions(cl, &err);
    if (!err.isEmpty())
        job->setErrorMessage(err);

/*
    QString path;
    if (job->shouldProceed()) {
        // no long-running operation can be done here.
        // "npackdcl path" must be fast.
        path = InstalledPackages::getDefault()->findPath_npackdcl(d);
    }

    if (job->shouldProceed() && path.isEmpty() && !package.contains('.')) {
        QString err = DBRepository::getDefault()->openDefault("default", true);
        if (err.isEmpty()) {
            Package* p = AbstractRepository::findOnePackage(package, &err);
            if (!err.isEmpty()) {
                delete p;
                p = 0;
            }

            if (p) {
                d.package = p->name;
                path = InstalledPackages::getDefault()->findPath_npackdcl(d);
            }

            delete p;
        } else {
            job->setErrorMessage(err);
        }
    }
    */

    if (err.isEmpty()) {
        QStringList paths;
        for (int i = 0; i < ipvs.size(); i++) {
            paths.append(ipvs.at(i)->getDirectory().replace('/', '\\'));
        }

        if (cl.isPresent("json")) {
            QJsonObject top;
            QJsonArray path;
            for (int i = 0; i < paths.size(); i++) {
                path.append(paths.at(i));
            }
            top["path"] = path;
            printJSON(top);
        } else if (cl.isPresent("cmd")) {
            for (int i = 0; i < paths.size(); i++) {
                WPMUtils::writeln(QString("set npackd_path") +
                        QString::number(i) + "=" + paths.at(i));
            }
        } else {
            for (int i = 0; i < paths.size(); i++) {
                WPMUtils::writeln(paths.at(i));
            }
        }
    }

    job->complete();
}

void App::place(Job* job)
{
    QString package = cl.get("package");
    QString version = cl.get("version");
    QString where = cl.get("file");

    if (job->shouldProceed()) {
        if (package.isNull()) {
            job->setErrorMessage("Missing option: --package");
        }
    }

    if (job->shouldProceed()) {
        if (!Package::isValidName(package)) {
            job->setErrorMessage("Invalid package name: " + package);
        }
    }

    if (job->shouldProceed()) {
        if (version.isNull()) {
            job->setErrorMessage("Missing option: --version");
        }
    }

    Version version_;
    if (job->shouldProceed()) {
        if (!version_.setVersion(version)) {
            job->setErrorMessage("Invalid version number: " + version);
        }
    }

    if (job->shouldProceed()) {
        if (where.isNull()) {
            job->setErrorMessage("Missing option: --file");
        }
    }

    if (job->shouldProceed()) {
        QFileInfo fi(where);
        if (!fi.exists()) {
            job->setErrorMessage("Directory does not exist: " + where);
        } else if (!fi.isDir()) {
            job->setErrorMessage("Not a directory: " + where);
        }
    }

    InstalledPackages* ip = InstalledPackages::getDefault();
    if (job->shouldProceed()) {
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.1);
        }
    }

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->openDefault();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.2);
        }
    }

    if (job->shouldProceed()) {
        QFileInfo fi(where);
        InstalledPackageVersion* f = ip->findOwner(fi.absoluteFilePath());
        if (f) {
            DBRepository* rep = DBRepository::getDefault();
            Package* p = rep->findPackage_(f->package);
            QString title = p ? p->title : "?";
            job->setErrorMessage(QString(
                    "%1 %2 (%3) is installed in \"%4\"").
                    arg(title).arg(f->version.getVersionString()).
                    arg(f->package).arg(f->directory));
            delete p;
            delete f;
        }
    }

    bool success = false;
    if (job->shouldProceed()) {
        QString err = InstalledPackages::getDefault()->setPackageVersionPath(
                package, version_, where);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.7);
            success = true;
        }
    }

    if (job->shouldProceed()) {
        DBRepository* rep = DBRepository::getDefault();

        Package* p = new Package(package, package);
        rep->savePackage(p, false);
        delete p;

        PackageVersion* pv = new PackageVersion(package, version_);
        rep->savePackageVersion(pv, false);
        delete pv;
    }

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->updateStatus(package);
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.99);
        }
    }

    if (success) {
        QString msg = QString(
                "The package %1 %2 was placed successfully in \"%3\"").
                arg(package, version, where);
        qCInfo(npackdImportant()).noquote() << msg;
    }

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();
}

void App::update(Job* job)
{
    CoInitialize(nullptr);
    DBRepository* rep = DBRepository::getDefault();
    job->setTitle("Updating packages");

    if (job->shouldProceed()) {
        QString err = InstalledPackages::getDefault()->readRegistryDatabase();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.05);
        }
    }

    QStringList urls_ = cl.getAll("url");
    QString user = cl.get("user");
    QString password = cl.get("password");
    QString proxyUser = cl.get("proxy-user");
    QString proxyPassword = cl.get("proxy-password");
    QString query = cl.get("query");

    DWORD programCloseType = WPMUtils::CLOSE_WINDOW;
    if (job->shouldProceed()) {
        QString err;
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        }
    }

    QTemporaryFile tempFile;

    if (job->shouldProceed()) {
        if (urls_.count() == 0) {
            QString err = rep->openDefault();
            if (!err.isEmpty())
                job->setErrorMessage(err);
            else
                job->setProgress(0.1);
        } else {
            QList<QUrl*> urls;

            for (int i = 0; i < urls_.count(); i++) {
                if (!job->shouldProceed())
                    break;

                QString url = urls_.at(i);
                QUrl* url_ = new QUrl();
                url_->setUrl(url, QUrl::TolerantMode);
                if (!url_->isValid()) {
                    job->setErrorMessage("Invalid URL: " + url);
                } else {
                    urls.append(url_);
                }
            }

            if (job->shouldProceed()) {
                if (!tempFile.open()) {
                    job->setErrorMessage(QObject::tr("Error creating a temporary file"));
                } else {
                    tempFile.close();
                }
            }

            if (job->shouldProceed()) {
                QString err = rep->open(QStringLiteral("tempdb"),
                        tempFile.fileName());
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                }
            }

            if (job->shouldProceed()) {
                Job* sub = job->newSubJob(0.10,
                        QObject::tr("Updating the temporary database"), true, true);
                rep->clearAndDownloadRepositories(sub, urls, interactive, user, password, proxyUser, proxyPassword, true, false);
            }

            qDeleteAll(urls);
            urls.clear();
        }
    }

    QStringList packages_;
    QStringList versions_;

    QList<CommandLine::ParsedOption*> parsed = cl.getParsedOptions();
    for (int i = 0; i < parsed.size(); ) {
        CommandLine::ParsedOption* po = parsed.at(i);
        if (po->opt && po->opt->name == "package") {
            packages_.append(po->value);
            i++;

            QString versions;
            if (i < parsed.size()) {
                po = parsed.at(i);
                if (po->opt && po->opt->name == "versions") {
                    versions = po->value;
                    i++;
                }
            }
            versions_.append(versions);
        } else if (po->opt && po->opt->name == "versions") {
            job->setErrorMessage("Missing option: --versions without --package");
            break;
        } else {
            i++;
        }
    }

    // process --query
    if (job->shouldProceed()) {
        if (!query.isNull()) {
            QString err;
            QStringList list = rep->findPackages(Package::Status::UPDATEABLE,
                    Package::Status::NOT_INSTALLED_NOT_AVAILABLE, query, -1, -1, &err);
            packages_.append(list);
            for (int i = 0; i < list.size(); i++) {
                versions_.append(QString());
            }
        }
    }

    if (job->shouldProceed()) {
        if (packages_.size() == 0) {
            job->setErrorMessage("Missing option: --package");
        }
    }

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            if (!Package::isValidName(package)) {
                job->setErrorMessage("Invalid package name: " + package);
            }
        }
    }

    QString file = cl.get("file");
    if (job->shouldProceed()) {
        if (!file.isNull()) {
            QDir d;
            file = WPMUtils::normalizePath(d.absoluteFilePath(file), false);

            if (packages_.size() != 1) {
                job->setErrorMessage(
                        "The installation directory can only be specified if one package should be updated.");
            }
        }
    }

    QList<Package*> toUpdate;
    QList<Dependency*> toUpdate2;

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            QString versions = versions_.at(i);

            QString err;
            Package* p = rep->findOnePackage(package, &err);
            if (!p) {
                qCDebug(npackd) << "did not found a package" << package;

                job->setErrorMessage(err);
            } else {
                qCDebug(npackd) << "found a package" << p->name;

                if (versions.isEmpty()) {
                    qCDebug(npackd) << "using the package" << p->name;

                    toUpdate.append(p);
                } else {
                    qCDebug(npackd) << "using version range" << versions;

                    Dependency* d = new Dependency();
                    if (!d->setVersions(versions)) {
                        delete d;
                        job->setErrorMessage(err);
                    } else {
                        // this package name could be different from the one in
                        // the command line if the short package name is used
                        d->package = p->name;

                        toUpdate2.append(d);

                        // WPMUtils::writeln(d->toString());
                    }

                    delete p;
                }
            }

            if (!job->shouldProceed())
                break;
        }
    }

    bool keepDirectories = cl.isPresent("keep-directories");
    bool install = cl.isPresent("install");

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->planAddMissingDeps(
                installed, ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    bool up2date = false;
    if (job->shouldProceed()) {
        qCDebug(npackd) << "planning updates for" << toUpdate.size() <<
                "packages and " << toUpdate2.size() << "version ranges";

        QString err = rep->planUpdates(installed, toUpdate, toUpdate2, ops,
                keepDirectories, install, file);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            qCDebug(npackd) << "the update plan contains" << ops.size() <<
                    "operations";

            job->setProgress(0.15);
            up2date = ops.size() == 0;
        }
    }

    /**
     *confirmation. This is not yet enabled in order to maintain the
     * compatibility.
    if (job->shouldProceed() && !up2date) {
        QString title;
        QString err;
        if (!confirm(ops, &title, &err))
            job->cancel();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }
    */

    if (job->shouldProceed() && !up2date) {
        Job* ijob = job->newSubJob(0.85, "Updating");
        processInstallOperations(ijob, rep, ops, programCloseType, interactive,
                user, password, proxyUser, proxyPassword);
        if (!ijob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(QString("Error updating: %1").
                    arg(ijob->getErrorMessage()));
        }
    }
    qDeleteAll(ops);

    if (job->shouldProceed()) {
        if (up2date) {
            WPMUtils::writeln("The packages are already up-to-date");
        } else if (job->getErrorMessage().isEmpty()) {
            qCInfo(npackdImportant()).noquote() <<
                    "The packages were updated successfully";
        }
    }

    qDeleteAll(toUpdate);
    qDeleteAll(toUpdate2);

    job->complete();
    CoUninitialize();
}

void App::processInstallOperations(Job *job,
        DBRepository* rep,
        const QList<InstallOperation *> &ops, DWORD programCloseType,
        bool interactive, const QString user, const QString password,
        const QString proxyUser, const QString proxyPassword)
{
    if (rep->includesRemoveItself(ops)) {
        QString newExe;

        if (job->shouldProceed()) {
            Job* sub = job->newSubJob(0.8, "Copying the executable");
            QString thisExe = WPMUtils::getExeFile();

            // 1. copy .exe to the temporary directory
            QTemporaryFile of(QDir::tempPath() + "\\npackdclXXXXXX.exe");
            of.setAutoRemove(false);
            if (!of.open())
                job->setErrorMessage(of.errorString());
            else {
                newExe = of.fileName();
                if (!of.remove()) {
                    job->setErrorMessage(of.errorString());
                } else {
                    of.close();

                    // qCDebug(npackd) << "self-update 1";

                    if (!QFile::copy(thisExe, newExe))
                        job->setErrorMessage("Error copying the binary");
                    else
                        sub->completeWithProgress();

                    // qCDebug(npackd) << "self-update 2";
                }
            }
        }

        QString batchFileName;
        if (job->shouldProceed()) {
            QString pct = WPMUtils::programCloseType2String(programCloseType);
            QStringList batch;
            for (int i = 0; i < ops.count(); i++) {
                InstallOperation* op = ops.at(i);
                QString oneCmd = "\"" + newExe + "\" ";

                // ping 1.1.1.1 always fails => we use || instead of &&
                if (op->install) {
                    oneCmd += "add -p " + op->package + " -v " +
                            op->version.getVersionString() +
                            " || ping 1.1.1.1 -n 1 -w 10000 > nul || exit /b %errorlevel%";
                } else {
                    oneCmd += "remove -p " + op->package + " -v " +
                            op->version.getVersionString() +
                            " -e " + pct +
                            " || ping 1.1.1.1 -n 1 -w 10000 > nul || exit /b %errorlevel%";
                }
                batch.append(oneCmd);
            }

            // qCDebug(npackd) << "self-update 3";

            QTemporaryFile file(QDir::tempPath() +
                              "\\npackdclXXXXXX.bat");
            file.setAutoRemove(false);
            if (!file.open())
                job->setErrorMessage(file.errorString());
            else {
                batchFileName = file.fileName();

                // qCDebug(npackd) << "batch" << file.fileName();

                QTextStream stream(&file);
                stream.setCodec("UTF-8");
                stream << batch.join("\r\n");
                if (stream.status() != QTextStream::Ok)
                    job->setErrorMessage("Error writing the .bat file");
                file.close();

                // qCDebug(npackd) << "self-update 4";

                job->setProgress(0.9);
            }
        }

        if (job->shouldProceed()) {
            Job* sub = job->newSubJob(0.1, "Starting the copied binary");
            QString file_ = batchFileName;
            file_.replace('/', '\\');
            QString prg = WPMUtils::findCmdExe();
            QString args = "/U /E:ON /V:OFF /C \"\"" + file_ + "\"\"";
            QString winDir = WPMUtils::getWindowsDir();

            WPMUtils::writeln(
                    (QObject::tr("Starting update process %1 with parameters %2")).
                    arg(prg).arg(args));

            args = "\"" + prg + "\" " + args;

            bool success = false;
            PROCESS_INFORMATION pinfo;

            STARTUPINFOW startupInfo = {
                sizeof(STARTUPINFO), nullptr, nullptr, nullptr,
                static_cast<DWORD>(CW_USEDEFAULT),
                static_cast<DWORD>(CW_USEDEFAULT),
                static_cast<DWORD>(CW_USEDEFAULT),
                static_cast<DWORD>(CW_USEDEFAULT),
                0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr
            };

            // we do to not use CREATE_UNICODE_ENVIRONMENT here to not start
            // new console if the current console is not Unicode, which is
            // normally the case if you start cmd.exe from the Windows start
            // menu
            success = CreateProcess(
                    WPMUtils::toLPWSTR(prg),
                    WPMUtils::toLPWSTR(args),
                    nullptr, nullptr, TRUE,
                    0 /*CREATE_UNICODE_ENVIRONMENT*/, nullptr,
                    WPMUtils::toLPWSTR(winDir), &startupInfo, &pinfo);

            if (success) {
                CloseHandle(pinfo.hThread);
                CloseHandle(pinfo.hProcess);
                // qCDebug(npackd) << "success!222";
            }

            // qCDebug(npackd) << "self-update 5";

            sub->completeWithProgress();
            job->setProgress(1);
        }

        job->complete();
    } else {
        rep->process(job, ops, programCloseType, debug, interactive, user,
                password, proxyUser, proxyPassword);
    }
}

void App::build(Job* job)
{
    job->setTitle("Building a package");

    DBRepository* rep = DBRepository::getDefault();
    if (job->shouldProceed()) {
        QString err = rep->openDefault();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        /* we ignore the returned error as it should also work for non-admins */
        addNpackdCL(rep);
    }

    InstalledPackages* ip = InstalledPackages::getDefault();

    if (job->shouldProceed()) {
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.1);
        }
    }

    std::unique_ptr<Dependency> sourceDep;

    if (job->shouldProceed()) {
        QString err;
        QList<Dependency*> pvs =
                PackageUtils::getPackageVersionOptions(cl, &err);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else if (pvs.size() != 1)
            job->setErrorMessage(
                    QObject::tr("Exactly one source package version should be specified"));
        else {
            sourceDep.reset(pvs.takeAt(0));
        }

        qDeleteAll(pvs);
    }

    std::unique_ptr<InstalledPackageVersion> source;
    if (job->shouldProceed()) {
        source.reset(ip->findHighestInstalledMatch(*sourceDep));
        if (!source)
            job->setErrorMessage(
                    QObject::tr("The installed source package version was not found"));
    }

    QString outputPackage;
    if (job->shouldProceed()) {
        outputPackage = cl.get("output-package");
        if (outputPackage.isNull()) {
            job->setErrorMessage("Missing option: --output-package");
        }
    }

    std::unique_ptr<PackageVersion> pv;
    if (job->shouldProceed()) {
        QString err;
        pv.reset(rep->findPackageVersion_(source->package, source->version,
                &err));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else if (!pv)
            job->setErrorMessage(QObject::tr("The source package version was not found"));
    }

    QString outputDir;
    if (job->shouldProceed()) {
        std::unique_ptr<PackageVersion> output(new PackageVersion(outputPackage,
                pv->version));
        outputDir = output->getPreferredInstallationDirectory();
        if (!QDir().mkpath(outputDir))
            job->setErrorMessage(QObject::tr("Cannot create the directory %1").
                    arg(outputDir));
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.8, QObject::tr("Build package"), true, true);
        pv->build(sub, outputPackage, outputDir, true);
    }

    if (job->shouldProceed()) {
        qCInfo(npackdImportant()).noquote() << QObject::tr(
                "The package %1 was built successfully in %2 to %3").arg(
                pv->toString()).arg(outputDir).arg(outputPackage);
    }

    job->complete();
}

void App::add(Job* job)
{
    CoInitialize(nullptr);
    job->setTitle("Installing packages");

    QStringList urls_ = cl.getAll("url");
    QString user = cl.get("user");
    QString password = cl.get("password");
    QString proxyUser = cl.get("proxy-user");
    QString proxyPassword = cl.get("proxy-password");

    DBRepository* dbr = DBRepository::getDefault();
    QTemporaryFile tempFile;

    if (job->shouldProceed()) {
        if (urls_.count() == 0) {
            QString err = dbr->openDefault();
            if (!err.isEmpty())
                job->setErrorMessage(err);
            else
                job->setProgress(0.1);
        } else {
            QList<QUrl*> urls;

            for (int i = 0; i < urls_.count(); i++) {
                if (!job->shouldProceed())
                    break;

                QString url = urls_.at(i);
                QUrl* url_ = new QUrl();
                url_->setUrl(url, QUrl::TolerantMode);
                if (!url_->isValid()) {
                    job->setErrorMessage("Invalid URL: " + url);
                } else {
                    urls.append(url_);
                }
            }

            if (job->shouldProceed()) {
                if (!tempFile.open()) {
                    job->setErrorMessage(QObject::tr("Error creating a temporary file"));
                } else {
                    tempFile.close();
                }
            }

            if (job->shouldProceed()) {
                QString err = dbr->open(QStringLiteral("tempdb"),
                        tempFile.fileName());
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                }
            }

            if (job->shouldProceed()) {
                Job* sub = job->newSubJob(0.10,
                        QObject::tr("Updating the temporary database"), true, true);
                dbr->clearAndDownloadRepositories(sub, urls, interactive, user, password, proxyUser, proxyPassword, true, false);
            }

            qDeleteAll(urls);
            urls.clear();
        }
    }

    if (job->shouldProceed()) {
        /* we ignore the returned error as it should also work for non-admins */
        addNpackdCL(dbr);
    }

    InstalledPackages* ip = InstalledPackages::getDefault();

    if (job->shouldProceed()) {
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.1);
        }
    }

    QList<PackageVersion*> toInstall;

    if (job->shouldProceed()) {
        QString err;
        toInstall = PackageUtils::getAddPackageVersionOptions(*dbr, cl, &err);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    QString file = cl.get("file");
    if (job->shouldProceed()) {
        if (!file.isNull()) {
            QDir d;
            file = WPMUtils::normalizePath(d.absoluteFilePath(file), false);

            if (toInstall.size() != 1) {
                job->setErrorMessage(
                        "The installation directory can only be specified if one package version should be installed.");
            } else {
                PackageVersion* pv = toInstall.at(0);
                QString ip_ = ip->getPath(pv->package, pv->version);

                if (pv->installed() && !WPMUtils::pathEquals(ip_, file)) {
                    job->setErrorMessage(QString(
                            "The package version %1 is already installed in \"%2\"").
                            arg(pv->toString(), ip_));
                }
            }
        }
    }

    // debug: WPMUtils::outputTextConsole << "Versions: " << d.toString()) << std::endl;
    QList<InstallOperation*> ops;
    InstalledPackages installed(*ip);

    if (job->shouldProceed()) {
        QString err;
        err = dbr->planAddMissingDeps(installed, ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        QString err;

        QList<PackageVersion*> avoid;
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            if (job->shouldProceed())
                err = pv->planInstallation(dbr, installed, ops, avoid, file);
            if (!err.isEmpty()) {
                job->setErrorMessage(err);
            }
        }
    }

    //qCInfo(npackd) << "err" << job->getErrorMessage();

    // debug: WPMUtils::outputTextConsole(QString("%1\r\n").arg(ops.size()));

    if (job->shouldProceed() && ops.size() > 0) {
        Job* ijob = job->newSubJob(0.8, "Installing");
        processInstallOperations(ijob, dbr, ops, WPMUtils::CLOSE_WINDOW,
                interactive, user, password, proxyUser, proxyPassword);
        if (!ijob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error installing: %1").
                    arg(ijob->getErrorMessage()));
    }

    if (job->shouldProceed()) {
        qCInfo(npackdImportant()).noquote() << "The packages were installed successfully";
    }

    qDeleteAll(ops);
    qDeleteAll(toInstall);

    job->complete();
    CoUninitialize();
}

bool App::confirm(const QList<InstallOperation*> install, QString* title,
        QString* err)
{
    *err = "";

    QString names;
    for (int i = 0; i < install.count(); i++) {
        InstallOperation* op = install.at(i);
        if (!op->install) {
            std::unique_ptr<PackageVersion> pv(op->findPackageVersion(err));
            if (!err->isEmpty())
                break;

            if (!names.isEmpty())
                names.append(", ");
            if (!pv)
                names.append(op->package + " " +
                        op->version.getVersionString());
            else
                names.append(pv->toString());
        }
    }

    QString installNames;
    if (err->isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            InstallOperation* op = install.at(i);
            if (op->install) {
                std::unique_ptr<PackageVersion> pv(op->findPackageVersion(err));
                if (!err->isEmpty())
                    break;

                if (!installNames.isEmpty())
                    installNames.append(", ");
                if (!pv)
                    installNames.append(op->package + " " +
                            op->version.getVersionString());
                else
                    installNames.append(pv->toString());
            }
        }
    }

    int installCount = 0, uninstallCount = 0;
    for (int i = 0; i < install.count(); i++) {
        InstallOperation* op = install.at(i);
        if (op->install)
            installCount++;
        else
            uninstallCount++;
    }

    bool b;
    QString msg;
    if (!err->isEmpty()) {
        b = false;
        *title = "Error";
    } else if (installCount == 1 && uninstallCount == 0) {
        b = true;
        *title = "Installing";
    } else if (installCount == 0 && uninstallCount == 1) {
        *title = "Uninstalling";
        InstallOperation* op0 = install.at(0);

        std::unique_ptr<PackageVersion> pv(op0->findPackageVersion(err));
        if (err->isEmpty()) {
            if (!pv)
                pv.reset(new PackageVersion(op0->package, op0->version));

            msg = QString("The package %1 will be uninstalled. "
                    "The corresponding directory %2 "
                    "will be completely deleted. "
                    "There is no way to restore the files. Are you sure (y/n)?:").
                    arg(pv->toString()).
                    arg(pv->getPath());
            b = WPMUtils::confirmConsole(msg);
        } else {
            b = false;
            *title = "Error";
        }
    } else if (installCount > 0 && uninstallCount == 0) {
        *title = QString("Installing %1 packages").arg(
                installCount);
        msg = QString("%1 package(s) will be installed: %2. Are you sure (y/n)?:").
                arg(installCount).arg(installNames);
        b = WPMUtils::confirmConsole(msg);
    } else if (installCount == 0 && uninstallCount > 0) {
        *title = QString("Uninstalling %1 packages").arg(
                uninstallCount);
        msg = QString("%1 package(s) will be uninstalled: %2. "
                "The corresponding directories "
                "will be completely deleted. "
                "There is no way to restore the files. Are you sure (y/n)?:").
                arg(uninstallCount).arg(names);
        b = WPMUtils::confirmConsole(msg);
    } else {
        *title = QString("Installing %1 packages, uninstalling %2 packages").arg(
                installCount).arg(uninstallCount);
        msg = QString("%1 package(s) will be uninstalled: %2 ("
                "the corresponding directories "
                "will be completely deleted; "
                "there is no way to restore the files) "
                "and %3 package(s) will be installed: %4. Are you sure (y/n)?:").
                arg(uninstallCount).
                arg(names).
                arg(installCount).
                arg(installNames);
        b = WPMUtils::confirmConsole(msg);
    }

    return b;
}

void App::remove(Job *job)
{
    job->setTitle("Removing packages");

    DBRepository* rep = DBRepository::getDefault();

    if (job->shouldProceed()) {
        QString err = rep->openDefault();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        /* we ignore the returned error as it should also work for non-admins */
        addNpackdCL(rep);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1,
                "Reading list of installed packages from the registry");
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    DWORD programCloseType = WPMUtils::CLOSE_WINDOW;
    if (job->shouldProceed()) {
        QString err;
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        }
    }

    QString err;
    QList<PackageVersion*> toRemove =
            PackageVersion::getRemovePackageVersionOptions(cl, &err);
    if (!err.isEmpty())
        job->setErrorMessage(err);

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());

    if (job->shouldProceed()) {
        err = rep->planAddMissingDeps(installed, ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        QString err;
        if (!err.isEmpty())
            job->setErrorMessage(err);

        if (job->shouldProceed()) {
            for (int i = 0; i < toRemove.size(); i++) {
                PackageVersion* pv = toRemove.at(i);
                err = rep->planUninstallation(installed,
                        pv->package, pv->version, ops);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                    break;
                }
            }
        }
    }

    /**
     *confirmation. This is not yet enabled in order to maintain the
     * compatibility.
    if (job->shouldProceed()) {
        QString title;
        QString err;
        if (!confirm(ops, &title, &err))
            job->cancel();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }
    */

    if (job->shouldProceed()) {
        Job* removeJob = job->newSubJob(0.9,
                "Removing");
        processInstallOperations(removeJob, rep, ops, programCloseType,
                interactive, "", "", "", "");
        if (!removeJob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error removing: %1\r\n").
                    arg(removeJob->getErrorMessage()));
    }

    if (job->shouldProceed()) {
        qCInfo(npackdImportant()).noquote() << "The packages were removed successfully";
    }

    qDeleteAll(ops);
    qDeleteAll(toRemove);

    job->complete();
}

void App::info(Job* job)
{
    bool json = cl.isPresent("json");

    job->setTitle("Showing information");

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->openDefault("default", true);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.01);
    }

    QString package = cl.get("package");
    QString version = cl.get("version");

    if (job->shouldProceed()) {
        if (package.isNull()) {
            job->setErrorMessage("Missing option: --package");
        }
    }

    if (job->shouldProceed()) {
        if (!Package::isValidName(package)) {
            job->setErrorMessage("Invalid package name: " + package);
        }
    }

    DBRepository* rep = DBRepository::getDefault();
    Package* p = nullptr;
    if (job->shouldProceed()) {
        QString r;
        p = rep->findOnePackage(package, &r);
        if (!r.isEmpty())
            job->setErrorMessage(r);
    }

    Version v;
    if (job->shouldProceed()) {
        // debug: WPMUtils::outputTextConsole <<  package) << " " << versions);
        if (!version.isNull()) {
            if (!v.setVersion(version)) {
                job->setErrorMessage("Cannot parse version: " + version);
            }
        }
    }

    PackageVersion* pv = nullptr;
    if (job->shouldProceed()) {
        if (!version.isNull()) {
            QString r;
            pv = rep->findPackageVersion_(p->name, v, &r);
            if (!r.isEmpty())
                job->setErrorMessage(r);
            else if (!pv) {
                job->setErrorMessage(QString("Package version %1 not found").
                        arg(v.getVersionString()));
            }
        }
    }

    if (job->shouldProceed()) {
        if (json) {
            QJsonObject top;
            if (pv)
                pv->toJSON(top);
            else
                p->toJSON(top);
            printJSON(top);
        } else {
            WPMUtils::writeln("Title: " +
                    p->title);
            if (pv)
                WPMUtils::writeln("Version: " +
                        pv->version.getVersionString());
            WPMUtils::writeln("Description: " + p->description);
            WPMUtils::writeln("License: " + p->license);

            if (pv) {
                WPMUtils::writeln("Installation path: " +
                        pv->getPath());

                InstalledPackages* ip = InstalledPackages::getDefault();
                InstalledPackageVersion* ipv = ip->find(pv->package, pv->version);
                WPMUtils::writeln("Detection info: " +
                        (ipv ? ipv->detectionInfo : ""));
                delete ipv;
            }
            WPMUtils::writeln("Internal package name: " +
                    p->name);
            if (pv) {
                WPMUtils::writeln("Status: " +
                        pv->getStatus(rep));
                WPMUtils::writeln("Download URL: " +
                        pv->download.toString(QUrl::FullyEncoded));
            }
            WPMUtils::writeln("Package home page: " + p->url);
            WPMUtils::writeln("Change log: " + p->getChangeLog());
            WPMUtils::writeln("Categories: " +
                    p->categories.join(", "));
            WPMUtils::writeln("Icon: " + p->getIcon());
            QList<QString> screenshots = p->links.values("screenshot");
            WPMUtils::writeln("Screen shots: " +
                    (screenshots.count() > 0 ? screenshots.at(0) : "n/a")
                    );
            for (int i = 1; i < screenshots.count(); i++) {
                WPMUtils::writeln("    " + screenshots.at(i));
            }

            if (pv) {
                QString type;
                switch (pv->type) {
                    case PackageVersion::Type::ONE_FILE:
                        type = "one-file";
                        break;
                    case PackageVersion::Type::INNO_SETUP:
                        type = "inno-setup";
                        break;
                    case PackageVersion::Type::NSIS:
                        type = "nsis";
                        break;
                    default:
                        type = "zip";
                }

                WPMUtils::writeln(QString("Type: ") + type);

                WPMUtils::writeln(QString("Hash sum: ") +
                        (pv->hashSumType == QCryptographicHash::Sha1 ?
                        "SHA-1" : "SHA-256") +
                        ": " + pv->sha1);

                QString details;
                for (int i = 0; i < pv->importantFiles.count(); i++) {
                    if (i != 0)
                        details.append("; ");
                    details.append(pv->importantFilesTitles.at(i));
                    details.append(" (");
                    details.append(pv->importantFiles.at(i));
                    details.append(")");
                }
                WPMUtils::writeln("Important files: " + details);
            }

            if (pv) {
                QString details;
                for (int i = 0; i < pv->files.count(); i++) {
                    if (i != 0)
                        details.append("; ");
                    details.append(pv->files.at(i)->path);
                }
                WPMUtils::writeln("Text files: " + details);
            }

            if (!pv) {
                QString versions, r;
                QList<PackageVersion*> pvs = rep->getPackageVersions_(
                        p->name, &r);
                if (r.isEmpty()) {
                    for (int i = 0; i < pvs.count(); i++) {
                        PackageVersion* opv = pvs.at(i);
                        if (i != 0)
                            versions.append(", ");
                        versions.append(opv->version.getVersionString());
                    }
                } else {
                    job->setErrorMessage(r);
                }
                qDeleteAll(pvs);
                WPMUtils::writeln("Versions: " + versions);
            }

            if (!pv) {
                InstalledPackages* ip = InstalledPackages::getDefault();
                QList<InstalledPackageVersion*> ipvs = ip->getByPackage(p->name);
                if (ipvs.count() > 0) {
                    WPMUtils::writeln(QString("%1 versions are installed:").
                            arg(ipvs.count()));
                    for (int i = 0; i < ipvs.count(); ++i) {
                        InstalledPackageVersion* ipv = ipvs.at(i);
                        if (!ipv->getDirectory().isEmpty())
                            WPMUtils::writeln("    " +
                                    ipv->version.getVersionString() +
                                    " in " + ipv->getDirectory());
                    }
                } else {
                    WPMUtils::writeln("No versions are installed");
                }
                qDeleteAll(ipvs);
            }
        }
    }    

    if (job->shouldProceed()) {
        if (pv) {
            if (json) {
                // nothing
            } else {
                WPMUtils::writeln("Dependency tree:");
                QString r = printDependencies(pv->installed(), "", 1, pv);
                if (!r.isEmpty())
                    job->setErrorMessage(r);
            }
        }
    }

    delete pv;

    job->complete();
}

QString App::printDependencies(bool onlyInstalled, const QString parentPrefix,
        int level, PackageVersion* pv)
{
    QString err;

    InstalledPackages* ip = InstalledPackages::getDefault();
    DBRepository* rep = DBRepository::getDefault();
    for (int i = 0; i < pv->dependencies.count(); ++i) {
        QString prefix;
        if (i == pv->dependencies.count() - 1)
            prefix = QString() + QChar(0x2514) + QChar(0x2500);
        else
            prefix = QString() + QChar(0x251c) + QChar(0x2500);

        Dependency* d = pv->dependencies.at(i);
        InstalledPackageVersion* ipv = ip->findHighestInstalledMatch(*d);

        PackageVersion* pvd = nullptr;

        QString s;
        if (ipv) {
            pvd = rep->
                    findPackageVersion_(ipv->package, ipv->version, &err);
        } else {
            pvd = rep->findBestMatchToInstall(*d,
                    QList<PackageVersion*>(), &err);
        }
        delete ipv;

        if (!err.isEmpty()) {
            delete pvd;
            break;
        }

        QChar before;
        if (!pvd) {
            s = QString("Missing dependency on %1").
                    arg(rep->toString(*d, true));
            before = ' ';
        } else {
            s = QString("%1 resolved to %2").
                    arg(rep->toString(*d, true)).
                    arg(pvd->version.getVersionString());
            if (!pvd->installed())
                s += " (not yet installed)";

            if (pvd->dependencies.count() > 0)
                // middle dot (Unicode)
                before = QChar(0xb7);
            else
                before = ' ';
        }

        WPMUtils::writeln(parentPrefix + prefix + before + s);

        if (pvd) {
            QString nestedPrefix;
            if (i == pv->dependencies.count() - 1)
                nestedPrefix = parentPrefix + "  ";
            else
                nestedPrefix = parentPrefix + QChar(0x2502) + " ";
            err = printDependencies(onlyInstalled,
                    nestedPrefix,
                    level + 1, pvd);
            delete pvd;

            if (!err.isEmpty())
                break;
        }
    }

    return err;
}

void App::detect(Job* job)
{
    job->setTitle("Detecting packages");

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->openDefault();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    QString user = cl.get("user");
    QString password = cl.get("password");
    QString proxyUser = cl.get("proxy-user");
    QString proxyPassword = cl.get("proxy-password");

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                "Reading list of installed packages from the registry");
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    QList<QUrl*> urls;
    if (job->shouldProceed()) {
        QString err;
        urls = PackageUtils::getRepositoryURLs(&err);
        if (!err.isEmpty())
            job->setErrorMessage(QObject::tr("Cannot load the list of repositories: %1").arg(err));
    }

    DBRepository* rep = DBRepository::getDefault();
    rep->clearAndDownloadRepositories(job, urls, interactive, user, password, proxyUser, proxyPassword,
            true);
    if (job->shouldProceed()) {
        qCInfo(npackdImportant()).noquote() <<
                "Package detection completed successfully";
    }

    qDeleteAll(urls);
    urls.clear();

    job->complete();
}

void App::removeSCP(Job *job)
{
    job->setTitle("Removing software from the Software Control Panel");

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
        cppm.ignoreMSIEntries = false;
        cppm.cleanPackageTitles = false;
        Job* scanJob = job->newSubJob(0.1, "Scanning for packages", true, true);
        cppm.scan(scanJob, &installed, &rep);
    }

    Package* found = nullptr;
    if (job->shouldProceed()) {
        if (title.startsWith('/')) {
            title = title.mid(1);

            Qt::CaseSensitivity cs = Qt::CaseInsensitive;
            if (title.endsWith('/')) {
                title = title.left(title.length() - 1);
                cs = Qt::CaseSensitive;
            } else if (title.endsWith("/i")) {
                title = title.left(title.length() - 2);
                cs = Qt::CaseInsensitive;
            } else {
                job->setErrorMessage("Invalid regular expression: " +
                        cl.get("title"));
            }

            if (job->shouldProceed()) {
                qCDebug(npackd) << "regular expression" << title;

                QRegExp re(title, cs);
                for (int i = 0; i < rep.packages.size(); i++) {
                    Package* p = rep.packages.at(i);
                    qCDebug(npackd) << p->title;
                    if (re.indexIn(p->title) >= 0) {
                        found = p;
                        break;
                    }
                }
            }
        } else {
            for (int i = 0; i < rep.packages.size(); i++) {
                Package* p = rep.packages.at(i);
                if (p->title == title) {
                    found = p;
                    break;
                }
            }
        }

        if (!found)
            job->setErrorMessage("Cannot find the package");
    }

    PackageVersion* pv = nullptr;
    if (job->shouldProceed()) {
        qCDebug(npackd) << "found package" << found->name;

        for (int i = 0; i < installed.size(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            qCDebug(npackd) << "installed" << ipv->package <<
                    ipv->version.getVersionString();

            // ipv->installed() ins not checked here as the installation
            // directory may be empty (unknown)
            if (ipv->package == found->name/* && ipv->installed()*/) {
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
        QTemporaryFile of(QDir::tempPath() + "\\nclXXXXXX.bat");
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
                    path, WPMUtils::getMessagesLog(), QStringList(), false);
        }
    }

    job->complete();
}
