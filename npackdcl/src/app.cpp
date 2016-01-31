#include <limits>
#include <math.h>

#include <QRegExp>
#include <QScopedPointer>
#include <QProcess>
#include <QMultiMap>

#include "app.h"
#include "wpmutils.h"
#include "commandline.h"
#include "downloader.h"
#include "installedpackages.h"
#include "installedpackageversion.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "hrtimer.h"

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

    qSort(items.begin(), items.end(), compareByPackageTitle);

    QStringList titles;
    for (int i = 0; i < list->count(); i++) {
        (*list)[i] = items.at(i).first;
        titles.append(items.at(i).second);
    }

    return titles;
}

int App::process()
{
    cl.add("bare-format", 'b', "bare format (no heading or summary)",
            "", false);
    cl.add("debug", 'd', "turn on the debug output", "", false);
    cl.add("end-process", 'e',
            "list of ways to close running applications (c=close, k=kill, s=disconnect from file shares). The default value is 'c'.",
            "[c][k][s]", false);
    cl.add("file", 'f', "file or directory", "file", false);
    cl.add("install", 'i',
            "install a package if it was not installed", "", false);
    cl.add("keep-directories", 'k',
            "use the same directories for updated packages", "", false,
            "update");
    cl.add("non-interactive", 'n',
            "assume that there is no user and do not ask for input", "", false);
    cl.add("package", 'p',
            "internal package name (e.g. com.example.Editor or just Editor)",
            "package", true);
    cl.add("query", 'q', "search terms (e.g. editor)",
            "search terms", false, "search");
    cl.add("versions", 'r', "versions range (e.g. [1.5,2))",
            "range", false);
    cl.add("status", 's', "filters package versions by status",
            "status", false);
    cl.add("url", 'u', "repository URL (e.g. https://www.example.com/Rep.xml)",
            "repository", false);
    cl.add("version", 'v', "version number (e.g. 1.5.12)",
            "version", false);

    QString err = cl.parse();
    if (!err.isEmpty()) {
        err = "Error: " + err;
    }

    // cl.dump();

    if (err.isEmpty()) {
        this->interactive = !cl.isPresent("non-interactive");

        this->debug = cl.isPresent("debug");

        if (debug) {
            clp.setUpdateRate(0);
            MySQLQuery::debug = true;
        }
    }

    QStringList fr = cl.getFreeArguments();

    if (!err.isEmpty()) {
        // nothing. The error will be processed later.
    } else if (fr.count() == 0) {
        err = "Missing command. Try npackdcl help";
    } else if (fr.count() > 1) {
        err = "Unexpected argument: " + fr.at(1);
    } else {
        const QString cmd = fr.at(0);

        QList<CommandLine::ParsedOption*> parsed = cl.getParsedOptions();
        for (int i = 0; i < parsed.count(); i++) {
            CommandLine::Option* opt = parsed.at(i)->opt;
            if (opt->allowedCommands.count() > 0) {
                // qDebug() << "1" << opt->allowedCommands.count();
                if (!opt->allowedCommands.contains(cmd)) {
                    err = "The option --" + opt->name +
                            " is not allowed for the command \"" + cmd + "\"";
                    break;
                }
            }
        }

        if (!err.isEmpty()) {
            // nothing
        } else if (cmd == "help") {
            usage();
        } else if (cmd == "path") {
            err = path();
        } else if (cmd == "place") {
            err = place();
        } else if (cmd == "remove" || cmd == "rm") {
            err = DBRepository::getDefault()->openDefault();
            if (err.isEmpty()) {
                /* we ignore the returned error as it should also work for non-admins */
                addNpackdCL();

                err = remove();
            }
        } else if (cmd == "add") {
            err = DBRepository::getDefault()->openDefault();
            if (err.isEmpty()) {
                /* we ignore the returned error as it should also work for non-admins */
                addNpackdCL();

                err = add();
            }
        } else if (cmd == "add-repo") {
            err = addRepo();
        } else if (cmd == "set-repo") {
            err = setRepo();
        } else if (cmd == "remove-repo") {
            err = removeRepo();
        } else if (cmd == "list-repos") {
            err = listRepos();
        } else if (cmd == "search") {
            err = DBRepository::getDefault()->openDefault("default", true);
            if (err.isEmpty())
                err = search();
        } else if (cmd == "check") {
            err = DBRepository::getDefault()->openDefault();
            if (err.isEmpty())
                err = check();
        } else if (cmd == "which") {
            err = DBRepository::getDefault()->openDefault("default", true);
            if (err.isEmpty())
                err = which();
        } else if (cmd == "where") {
            err = where();
        } else if (cmd == "list") {
            err = DBRepository::getDefault()->openDefault("default", true);
            if (err.isEmpty())
                err = list();
        } else if (cmd == "info") {
            err = DBRepository::getDefault()->openDefault("default", true);
            if (err.isEmpty())
                err = info();
        } else if (cmd == "update") {
            err = DBRepository::getDefault()->openDefault();
            if (err.isEmpty())
                err = update();
        } else if (cmd == "detect") {
            err = DBRepository::getDefault()->openDefault();
            if (err.isEmpty())
                err = detect();
        } else if (cmd == "set-install-dir") {
            err = setInstallPath();
        } else if (cmd == "install-dir") {
            err = getInstallPath();
        } else {
            err = "Wrong command: " + cmd + ". Try npackdcl help";
        }
    }

    int r = 0;
    if (err.isEmpty())
        r = 0;
    else {
        r = 1;
        WPMUtils::writeln(err, false);
    }

    QCoreApplication::instance()->exit(r);

    return r;
}

QString App::addNpackdCL()
{
    QString err;

    AbstractRepository* r = AbstractRepository::getDefault_();
    Version myVersion;
    (void) myVersion.setVersion(NPACKD_VERSION);
    PackageVersion* pv = r->findPackageVersion_(
            "com.googlecode.windows-package-manager.NpackdCL",
            myVersion, &err);
    if (!pv) {
        pv = new PackageVersion(
                "com.googlecode.windows-package-manager.NpackdCL");
        pv->version = myVersion;
        r->savePackageVersion(pv);
    }
    delete pv;
    pv = 0;

    err = r->updateNpackdCLEnvVar();

    return err;
}

void App::usage()
{
    WPMUtils::writeln(QString(
            "ncl %1 - Npackd command line tool").
            arg(NPACKD_VERSION));
    const char* lines[] = {
        "Usage:",
        "    ncl add (--package=<package>",
        "            [--version=<version> | --versions=<versions>])+ ",
        "            [--non-interactive] [--file=<installation directory>]",
        "        installs packages. The newest available version will be ",
        "        installed, if none is specified.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    ncl add-repo --url=<repository>",
        "        appends a repository to the list",
        "    ncl check",
        "        checks the installed packages for missing dependencies",
        "    ncl detect [--non-interactive]",
        "        download repositories and detect packages from the MSI ",
        "        database and software control panel",
        "    ncl info --package=<package> [--version=<version>]",
        "        shows information about the specified package or package",
        "        version",
        "    ncl install-dir",
        "        prints the directory where packages will be installed",
        "    ncl list [--status=installed | all] [--bare-format]",
        "        lists package versions sorted by package name and version.",
        "        Please note that since 1.18 only installed package versions",
        "        are listed regardless of the --status switch.",
        "    ncl list-repos [--bare-format]",
        "        list currently defined repositories",
        "    ncl help",
        "        prints this help",
        "    ncl path --package=<package>",
        "            [--version=<version> | --versions=<versions>]",
        "        searches for an installed package and prints its location",
        "    ncl place --package=<package>",
        "            --version=<version> --file=<directory>",
        "        registers a package version installed without Npackd",
        "    ncl remove|rm (--package=<package> [--version=<version>])+",
        "           [--end-process=<types>]",
        "        removes packages. The version number may be omitted, ",
        "        if only one is installed.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    ncl remove-repo --url=<repository>",
        "        removes a repository from the list",
        "    ncl set-repo (--url=<repository>)+",
        "        changes the currently defined repositories",
        "    ncl search [--query=<search terms>] [--status=installed | all]",
        "            [--bare-format]",
        "        full text search. Lists found packages sorted by package name.",
        "        All packages are shown by default.",
        "    ncl set-install-dir --file=<directory>",
        "        changes the directory where packages will be installed",
        "    ncl update (--package=<package>)+ [--end-process=<types>]",
        "            [--install] [--keep-directories]",
        "            [--non-interactive] [--file=<installation directory>]",
        "        updates packages by uninstalling the currently installed",
        "        and installing the newest version. ",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    ncl where --file=<relative path>",
        "        finds all installed packages with the specified file or",
        "            directory",
        "    ncl which --file=<file>",
        "        finds the package that owns the specified file or directory",
        "Options:",
    };
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        WPMUtils::writeln(QString(lines[i]));
    }
    QStringList opts = this->cl.printOptions();
    for (int i = 0; i < opts.count(); i++) {
        WPMUtils::writeln(opts.at(i));
    }

    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occures.",
        "If the output is redirected, the texts will be encoded as UTF-8.",
        "",
        "See https://github.com/tim-lebedkov/npackd/wiki/CommandLine for more details.",
    };
    for (int i = 0; i < (int) (sizeof(lines2) / sizeof(lines2[0])); i++) {
        WPMUtils::writeln(QString(lines2[i]));
    }
}

QString App::listRepos()
{
    bool bare = cl.isPresent("bare-format");

    QString err;

    QList<QUrl*> urls = AbstractRepository::getRepositoryURLs(&err);
    if (err.isEmpty()) {
        if (!bare) {
            WPMUtils::writeln(QString("%1 repositories are defined:").
                    arg(urls.size()));
            WPMUtils::writeln("");
        }
        for (int i = 0; i < urls.size(); i++) {
            WPMUtils::writeln(urls.at(i)->toString());
        }
    }
    qDeleteAll(urls);

    return err;
}

QString App::getInstallPath()
{
    QString err;

    WPMUtils::outputTextConsole(WPMUtils::getInstallationDirectory());

    return err;
}

QString App::which()
{
    QString r;

    InstalledPackages* ip = InstalledPackages::getDefault();
    r = ip->readRegistryDatabase();

    QString file = cl.get("file");
    if (r.isEmpty()) {
        if (file.isNull()) {
            r = "Missing option: --file";
        }
    }

    if (r.isEmpty()) {
        QFileInfo fi(file);
        InstalledPackageVersion* f = ip->findOwner(fi.absoluteFilePath());
        if (f) {
            AbstractRepository* rep = AbstractRepository::getDefault_();
            Package* p = rep->findPackage_(f->package);
            QString title = p ? p->title : "?";
            WPMUtils::writeln(QString(
                    "%1 %2 (%3) is installed in \"%4\"").
                    arg(title).arg(f->version.getVersionString()).
                    arg(f->package).arg(f->directory));
            delete p;
            delete f;
        } else
            WPMUtils::writeln(QString("No package found for \"%1\"").
                    arg(file));
    }

    return r;
}

QString App::where()
{
    QString r;

    InstalledPackages* ip = InstalledPackages::getDefault();
    r = ip->readRegistryDatabase();

    QString file = cl.get("file");
    if (r.isEmpty()) {
        if (file.isNull()) {
            r = "Missing option: --file";
        }
    }

    if (r.isEmpty()) {
        QStringList paths = ip->getAllInstalledPackagePaths();
        for (int i = 0; i < paths.count(); i++) {
            QFileInfo fi(paths[i], file);
            if (fi.exists())
                WPMUtils::writeln(paths[i] + "\\" + file);
        }
    }

    return r;
}

QString App::setInstallPath()
{
    QString r;

    QString file = cl.get("file");
    if (r.isEmpty()) {
        if (file.isNull()) {
            r = "Missing option: --file";
        }
    }

    if (r.isEmpty()) {
        QFileInfo fi(file);
        file = fi.absoluteFilePath();
        file = WPMUtils::normalizePath(file, false);
    }

    if (r.isEmpty()) {
        r = AbstractRepository::checkInstallationDirectory(file);
    }

    if (r.isEmpty()) {
        r = WPMUtils::setInstallationDirectory(file);
    }

    return r;
}

QString App::check()
{
    Job* job = clp.createJob();
    job->setTitle("Checking dependency integrity for the installed packages");

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
        InstalledPackages::getDefault()->refresh(DBRepository::getDefault(),
                sub);
    }

    AbstractRepository* rep = AbstractRepository::getDefault_();
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
                if (!d->isInstalled()) {
                    WPMUtils::writeln(QString(
                            "%1 depends on %2, which is not installed").
                            arg(pv->toString(true)).
                            arg(d->toString(true)));
                    n++;
                }
            }
        }

        if (n == 0)
            WPMUtils::writeln("All dependencies are installed");

    }

    qDeleteAll(list);

    QString r = job->getErrorMessage();
    delete job;

    return r;
}

QString App::addRepo()
{
    QString err;

    QString url = cl.get("url").trimmed();

    if (err.isEmpty()) {
        if (url.isNull()) {
            err = "Missing option: --url";
        }
    }

    QUrl* url_ = 0;
    if (err.isEmpty()) {
        url_ = new QUrl();
        url_->setUrl(url, QUrl::StrictMode);
        if (!url_->isValid()) {
            err = "Invalid URL: " + url;
        }
    }

    if (err.isEmpty()) {
        QList<QUrl*> urls = AbstractRepository::getRepositoryURLs(&err);
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
                url_ = 0;
                AbstractRepository::setRepositoryURLs(urls, &err);
                if (err.isEmpty())
                    WPMUtils::writeln("The repository was added successfully");
            }
        }
        qDeleteAll(urls);
    }

    delete url_;

    return err;
}

QString App::setRepo()
{
    QString err;

    QStringList urls_ = cl.getAll("url");

    if (err.isEmpty()) {
        if (urls_.count() == 0) {
            err = "Missing option: --url";
        }
    }

    if (err.isEmpty()) {
        QList<QUrl*> urls;
        for (int i = 0; i < urls_.count(); i++) {
            if (!err.isEmpty())
                break;

            QString url = urls_.at(i);
            QUrl* url_ = new QUrl();
            url_->setUrl(url, QUrl::StrictMode);
            if (!url_->isValid()) {
                err = "Invalid URL: " + url;
            } else {
                urls.append(url_);
            }
        }

        if (err.isEmpty()) {
            AbstractRepository::setRepositoryURLs(urls, &err);
            if (err.isEmpty())
                WPMUtils::writeln("The repositories were changed successfully");
        }

        qDeleteAll(urls);
    }

    return err;
}

QString App::list()
{
    QString err;

    bool bare = cl.isPresent("bare-format");

    Job* job;
    if (bare)
        job = new Job();
    else
        job = clp.createJob();

    job->setTitle("Listing package versions");

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
        AbstractRepository* rep = AbstractRepository::getDefault_();
        list = rep->getInstalled_(&err);
        if (err.isEmpty()) {
            titles = sortPackageVersionsByPackageTitle(&list);
            sub->completeWithProgress();
            job->setProgress(1);
        }
    }

    if (err.isEmpty()) {
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

    qDeleteAll(list);

    delete job;

    return err;
}

QString App::search()
{
    bool bare = cl.isPresent("bare-format");
    QString query = cl.get("query");

    Job* job = clp.createJob();
    job->setTitle("Searching for packages");

    bool onlyInstalled = false;
    if (job->shouldProceed()) {
        QString status = cl.get("status");
        if (!status.isNull()) {
            if (status == "all")
                onlyInstalled = false;
            else if (status == "installed")
                onlyInstalled = true;
            else {
                job->setErrorMessage("Wrong status: " + status);
            }
        }
    }

    DBRepository* rep = DBRepository::getDefault();

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
        packageNames = rep->findPackages(Package::INSTALLED, onlyInstalled,
                query, -1, -1, &err);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01, "Fetching packages");
        QString err;
        list = rep->findPackages(packageNames);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        qSort(list.begin(), list.end(), packageLessThan);

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

        qDeleteAll(list);
        job->setProgress(1);
    }

    job->complete();
    QString err = job->getErrorMessage();
    delete job;

    return err;
}

QString App::removeRepo()
{
    QString err;

    QString url = cl.get("url");

    if (err.isEmpty()) {
        if (url.isNull()) {
            err = "Missing option: --url";
        }
    }

    QUrl* url_ = 0;
    if (err.isEmpty()) {
        url_ = new QUrl();
        url_->setUrl(url, QUrl::StrictMode);
        if (!url_->isValid()) {
            err = "Invalid URL: " + url;
        }
    }

    if (err.isEmpty()) {
        QList<QUrl*> urls = AbstractRepository::getRepositoryURLs(&err);
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
                AbstractRepository::setRepositoryURLs(urls, &err);
                if (err.isEmpty())
                    WPMUtils::writeln(
                            "The repository was removed successfully");
            }
        }
        qDeleteAll(urls);
    }

    delete url_;

    return err;
}

QString App::path()
{
    Job* job = new Job();

    QString package = cl.get("package");
    QString versions = cl.get("versions");
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

    Dependency d;
    if (job->shouldProceed()) {
        // debug: WPMUtils::outputTextConsole <<  package) << " " << versions);
        d.package = package;
        if (version.isNull()) {
            if (versions.isNull()) {
                d.min.setVersion(0, 0);
                d.max.setVersion(std::numeric_limits<int>::max(), 0);
            } else {
                if (!d.setVersions(versions)) {
                    job->setErrorMessage("Cannot parse versions: " +
                            versions);
                }
            }
        } else {
            if (d.min.setVersion(version)) {
                d.max = d.min;
                d.minIncluded = true;
                d.maxIncluded = true;
            } else
                job->setErrorMessage("Cannot parse version: " +
                        version);
        }
    }

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

    if (!path.isEmpty()) {
        path.replace('/', '\\');
        WPMUtils::writeln(path);
    }

    job->complete();

    QString r = job->getErrorMessage();

    delete job;

    return r;
}

QString App::place()
{
    Job* job = new Job();

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
            AbstractRepository* rep = AbstractRepository::getDefault_();
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
                "The package %1 %2 was placed successfully in %3").
                arg(package, version, where);
        WPMUtils::writeln(msg);
        WPMUtils::reportEvent(msg);
    }

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();

    QString r = job->getErrorMessage();

    delete job;

    return r;
}

QString App::update()
{
    DBRepository* rep = DBRepository::getDefault();
    Job* job = clp.createJob();
    job->setTitle("Updating packages");

    if (job->shouldProceed()) {
        QString err = InstalledPackages::getDefault()->readRegistryDatabase();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.05);
        }
    }

    int programCloseType = WPMUtils::CLOSE_WINDOW;
    if (job->shouldProceed()) {
        QString err;
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        }
    }

    QStringList packages_ = cl.getAll("package");

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

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            QString err;
            Package* p = AbstractRepository::findOnePackage(package, &err);
            if (p)
                toUpdate.append(p);
            else
                job->setErrorMessage(err);

            if (!job->shouldProceed())
                break;
        }
    }

    bool keepDirectories = cl.isPresent("keep-directories");
    bool install = cl.isPresent("install");

    QList<InstallOperation*> ops;
    bool up2date = false;
    if (job->shouldProceed()) {
        QString err = rep->planUpdates(toUpdate, ops, keepDirectories, install,
                file);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
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
        processInstallOperations(ijob, ops, programCloseType, interactive);
        if (!ijob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(QString("Error updating: %1").
                    arg(ijob->getErrorMessage()));
        }
    }
    qDeleteAll(ops);

    job->complete();

    QString r = job->getErrorMessage();
    if (job->isCancelled()) {
        r = "The package update was cancelled";
    } else if (up2date) {
        WPMUtils::writeln("The packages are already up-to-date");
    } else if (r.isEmpty()) {
        WPMUtils::writeln("The packages were updated successfully");
    }

    delete job;

    qDeleteAll(toUpdate);

    return r;
}

void App::processInstallOperations(Job *job,
        const QList<InstallOperation *> &ops, DWORD programCloseType,
        bool interactive)
{
    DBRepository* rep = DBRepository::getDefault();

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

                    // qDebug() << "self-update 1";

                    if (!QFile::copy(thisExe, newExe))
                        job->setErrorMessage("Error copying the binary");
                    else
                        sub->completeWithProgress();

                    // qDebug() << "self-update 2";
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

            // qDebug() << "self-update 3";

            QTemporaryFile file(QDir::tempPath() +
                              "\\npackdclXXXXXX.bat");
            file.setAutoRemove(false);
            if (!file.open())
                job->setErrorMessage(file.errorString());
            else {
                batchFileName = file.fileName();

                // qDebug() << "batch" << file.fileName();

                QTextStream stream(&file);
                stream.setCodec("UTF-8");
                stream << batch.join("\r\n");
                if (stream.status() != QTextStream::Ok)
                    job->setErrorMessage("Error writing the .bat file");
                file.close();

                // qDebug() << "self-update 4";

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
                sizeof(STARTUPINFO), 0, 0, 0,
                (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
                (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };

            // we do to not use CREATE_UNICODE_ENVIRONMENT here to not start
            // new console if the current console is not Unicode, which is
            // normally the case if you start cmd.exe from the Windows start
            // menu
            success = CreateProcess(
                    (wchar_t*) prg.utf16(),
                    (wchar_t*) args.utf16(),
                    0, 0, TRUE,
                    0 /*CREATE_UNICODE_ENVIRONMENT*/, 0,
                    (wchar_t*) winDir.utf16(), &startupInfo, &pinfo);

            if (success) {
                CloseHandle(pinfo.hThread);
                CloseHandle(pinfo.hProcess);
                // qDebug() << "success!222";
            }

            // qDebug() << "self-update 5";

            sub->completeWithProgress();
            job->setProgress(1);
        }

        job->complete();
    } else {
        rep->process(job, ops, programCloseType, debug, interactive);
    }
}

QString App::add()
{
    Job* job = clp.createJob();
    job->setTitle("Installing packages");

    InstalledPackages* ip = InstalledPackages::getDefault();

    if (job->shouldProceed()) {
        QString err = InstalledPackages::getDefault()->readRegistryDatabase();
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        } else {
            job->setProgress(0.1);
        }
    }

    QString err;
    QList<PackageVersion*> toInstall =
            PackageVersion::getAddPackageVersionOptions(cl, &err);
    if (!err.isEmpty())
        job->setErrorMessage(err);

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
                            "The package version %1 is already installed in %2.").
                            arg(pv->toString(), ip_));
                }
            }
        }
    }

    // debug: WPMUtils::outputTextConsole << "Versions: " << d.toString()) << std::endl;
    QList<InstallOperation*> ops;
    if (job->shouldProceed()) {
        QString err;
        QList<PackageVersion*> installed =
                AbstractRepository::getDefault_()->getInstalled_(&err);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        QList<PackageVersion*> avoid;
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            if (job->shouldProceed())
                err = pv->planInstallation(installed, ops, avoid, file);
            if (!err.isEmpty()) {
                job->setErrorMessage(err);
            }
        }
        qDeleteAll(installed);
    }

    // debug: WPMUtils::outputTextConsole(QString("%1\r\n").arg(ops.size()));

    if (job->shouldProceed() && ops.size() > 0) {
        Job* ijob = job->newSubJob(0.9, "Installing");
        processInstallOperations(ijob, ops, WPMUtils::CLOSE_WINDOW,
                interactive);
        if (!ijob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error installing: %1").
                    arg(ijob->getErrorMessage()));
    }

    job->complete();

    QString r = job->getErrorMessage();
    if (r.isEmpty()) {
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            WPMUtils::writeln(QString(
                    "The package %1 was installed successfully in %2").arg(
                    pv->toString()).arg(pv->getPath()));
        }
    }

    delete job;

    qDeleteAll(ops);
    qDeleteAll(toInstall);

    return r;
}

bool App::confirm(const QList<InstallOperation*> install, QString* title,
        QString* err)
{
    *err = "";

    QString names;
    for (int i = 0; i < install.count(); i++) {
        InstallOperation* op = install.at(i);
        if (!op->install) {
            QScopedPointer<PackageVersion> pv(op->findPackageVersion(err));
            if (!err->isEmpty())
                break;

            if (!names.isEmpty())
                names.append(", ");
            if (pv.isNull())
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
                QScopedPointer<PackageVersion> pv(op->findPackageVersion(err));
                if (!err->isEmpty())
                    break;

                if (!installNames.isEmpty())
                    installNames.append(", ");
                if (pv.isNull())
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

        QScopedPointer<PackageVersion> pv(op0->findPackageVersion(err));
        if (err->isEmpty()) {
            if (pv.isNull())
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

QString App::remove()
{
    Job* job = clp.createJob();
    job->setTitle("Removing packages");

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

    int programCloseType = WPMUtils::CLOSE_WINDOW;
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

    AbstractRepository* ar = AbstractRepository::getDefault_();
    QList<InstallOperation*> ops;
    if (job->shouldProceed()) {
        QString err;
        QList<PackageVersion*> installed = ar->getInstalled_(&err);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        if (job->shouldProceed()) {
            for (int i = 0; i < toRemove.size(); i++) {
                PackageVersion* pv = toRemove.at(i);
                err = pv->planUninstallation(installed, ops);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                    break;
                }
            }
        }
        qDeleteAll(installed);
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
        processInstallOperations(removeJob, ops, programCloseType);
        if (!removeJob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error removing: %1\r\n").
                    arg(removeJob->getErrorMessage()));
    }

    job->complete();

    QString r = job->getErrorMessage();
    if (job->isCancelled()) {
        r = "The package removal was cancelled";
    } else if (r.isEmpty()) {
        WPMUtils::writeln("The packages were removed successfully");
    }

    delete job;

    qDeleteAll(ops);
    qDeleteAll(toRemove);

    return r;
}

QString App::info()
{
    QString r;

    Job* job = clp.createJob();
    job->setTitle("Showing information");

    if (job->shouldProceed()) {
        InstalledPackages* ip = InstalledPackages::getDefault();
        QString err = ip->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.01);
    }

    delete job;

    QString package = cl.get("package");
    QString version = cl.get("version");

    if (r.isEmpty()) {
        if (package.isNull()) {
            r = "Missing option: --package";
        }
    }

    if (r.isEmpty()) {
        if (!Package::isValidName(package)) {
            r = "Invalid package name: " + package;
        }
    }

    DBRepository* rep = DBRepository::getDefault();
    Package* p = 0;
    if (r.isEmpty()) {
        p = AbstractRepository::findOnePackage(package, &r);
    }

    Version v;
    if (r.isEmpty()) {
        // debug: WPMUtils::outputTextConsole <<  package) << " " << versions);
        if (!version.isNull()) {
            if (!v.setVersion(version)) {
                r = "Cannot parse version: " + version;
            }
        }
    }

    PackageVersion* pv = 0;
    if (r.isEmpty()) {
        if (!version.isNull()) {
            pv = rep->findPackageVersion_(p->name, v, &r);
            if (r.isEmpty() && !pv) {
                r = QString("Package version %1 not found").
                        arg(v.getVersionString());
            }
        }
    }

    if (r.isEmpty()) {
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
                    pv->getStatus());
            WPMUtils::writeln("Download URL: " +
                    pv->download.toString());
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
            WPMUtils::writeln(QString("Type: ") +
                    (pv->type == 0 ? "zip" : "one-file"));

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
            QString versions;
            QList<PackageVersion*> pvs = rep->getPackageVersions_(p->name, &r);
            for (int i = 0; i < pvs.count(); i++) {
                PackageVersion* opv = pvs.at(i);
                if (i != 0)
                    versions.append(", ");
                versions.append(opv->version.getVersionString());
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


    if (r.isEmpty()) {
        if (pv) {
            WPMUtils::writeln("Dependency tree:");
            r = printDependencies(pv->installed(), "", 1, pv);
        }
    }

    delete pv;

    return r;
}

QString App::printDependencies(bool onlyInstalled, const QString parentPrefix,
        int level, PackageVersion* pv)
{
    QString err;

    for (int i = 0; i < pv->dependencies.count(); ++i) {
        QString prefix;
        if (i == pv->dependencies.count() - 1)
            prefix = (QString() + ((QChar)0x2514) + ((QChar)0x2500));
        else
            prefix = (QString() + ((QChar)0x251c) + ((QChar)0x2500));

        Dependency* d = pv->dependencies.at(i);
        InstalledPackageVersion* ipv = d->findHighestInstalledMatch();

        PackageVersion* pvd = 0;

        QString s;
        if (ipv) {
            pvd = AbstractRepository::getDefault_()->
                    findPackageVersion_(ipv->package, ipv->version, &err);
        } else {
            pvd = d->findBestMatchToInstall(QList<PackageVersion*>(), &err);
        }
        delete ipv;

        if (!err.isEmpty()) {
            delete pvd;
            break;
        }

        QChar before;
        if (!pvd) {
            s = QString("Missing dependency on %1").
                    arg(d->toString(true));
            before = ' ';
        } else {
            s = QString("%1 resolved to %2").
                    arg(d->toString(true)).
                    arg(pvd->version.getVersionString());
            if (!pvd->installed())
                s += " (not yet installed)";

            if (pvd->dependencies.count() > 0)
                before = (QChar) 0xb7;
            else
                before = ' ';
        }

        WPMUtils::writeln(parentPrefix + prefix + before + s);

        if (pvd) {
            QString nestedPrefix;
            if (i == pv->dependencies.count() - 1)
                nestedPrefix = parentPrefix + "  ";
            else
                nestedPrefix = parentPrefix + ((QChar)0x2502) + " ";
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

QString App::detect()
{
    Job* job = clp.createJob();
    job->setTitle("Detecting packages");

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

    DBRepository* rep = DBRepository::getDefault();
    rep->updateF5(job, interactive);
    QString r = job->getErrorMessage();
    if (job->getErrorMessage().isEmpty()) {
        WPMUtils::writeln("Package detection completed successfully");
    }
    delete job;

    return r;
}
