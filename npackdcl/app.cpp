#include <limits>
#include <math.h>

#include <QRegExp>
#include <QScopedPointer>

#include "app.h"
#include "..\wpmcpp\wpmutils.h"
#include "..\wpmcpp\commandline.h"
#include "..\wpmcpp\downloader.h"
#include "..\wpmcpp\installedpackages.h"
#include "..\wpmcpp\installedpackageversion.h"
#include "..\wpmcpp\abstractrepository.h"
#include "..\wpmcpp\dbrepository.h"
#include "..\wpmcpp\hrtimer.h"

static bool packageVersionLessThan(const PackageVersion* pv1,
        const PackageVersion* pv2)
{
    if (pv1->package == pv2->package)
        return pv1->version.compare(pv2->version) < 0;
    else {
        QString pt1 = pv1->getPackageTitle();
        QString pt2 = pv2->getPackageTitle();
        return pt1.toLower() < pt2.toLower();
    }
}

bool packageLessThan(const Package* p1, const Package* p2)
{
    return p1->title.toLower() < p2->title.toLower();
}

#ifdef TEST
int App::test()
{
    Version a;
    Version b;
    b.setVersion("1.0.0");
    if (a != b)
        return 1;
    qDebug() << ".";

    a.setVersion("4.5.6.7.8.9.10");
    if (a.getVersionString() != "4.5.6.7.8.9.10")
        return 1;
    qDebug() << ".";

    a.setVersion("1.1");
    if (a != Version(1, 1))
        return 1;
    qDebug() << ".";

    a.setVersion("5.0.0.1");
    Version c(a);
    if (c.getVersionString() != "5.0.0.1")
        return 1;
    qDebug() << ".";

    Version* d = new Version();
    d->setVersion(7, 8, 9, 10);
    delete d;
    qDebug() << ".";

    a.setVersion(1, 17);
    if (a.getVersionString() != "1.17")
        return 1;
    qDebug() << ".";

    a.setVersion(2, 18, 3);
    if (a.getVersionString() != "2.18.3")
        return 1;
    qDebug() << ".";

    a.setVersion(3, 1, 3, 8);
    if (a.getVersionString() != "3.1.3.8")
        return 1;
    qDebug() << ".";

    a.setVersion("17.2.8.4");
    a.prepend(8);
    a.prepend(38);
    a.prepend(0);
    if (a.getVersionString() != "0.38.8.17.2.8.4")
        return 1;
    qDebug() << ".";

    a.setVersion("2.8.3");
    if (a.getVersionString(7) != "2.8.3.0.0.0.0")
        return 1;
    qDebug() << ".";

    a.setVersion("17.2");
    if (a.getNParts() != 2)
        return 1;
    qDebug() << ".";

    a.setVersion("8.4.0.0.0");
    a.normalize();
    if (a.getVersionString() != "8.4")
        return 1;
    if (!a.isNormalized())
        return 1;
    qDebug() << ".";

    if (Version("2.8.7.4.8.9") <= Version("2.8.6.4.8.8"))
        return 1;
    qDebug() << ".";

    qDebug() << "OK";
    return 0;
}
#endif

int App::process()
{
    QString err = DBRepository::getDefault()->open();
    if (!err.isEmpty()) {
        WPMUtils::outputTextConsole("Error: " + err + "\n");
        return 1;
    }

    /* we ignore the returned error as it should also work for non-admins */
    addNpackdCL();

    cl.add("package", 'p',
            "internal package name (e.g. com.example.Editor or just Editor)",
            "package", true);
    cl.add("versions", 'r', "versions range (e.g. [1.5,2))",
            "range", false);
    cl.add("version", 'v', "version number (e.g. 1.5.12)",
            "version", false);
    cl.add("url", 'u', "repository URL (e.g. https://www.example.com/Rep.xml)",
            "repository", false);
    cl.add("status", 's', "filters package versions by status",
            "status", false);
    cl.add("bare-format", 'b', "bare format (no heading or summary)",
            "", false);
    cl.add("query", 'q', "search terms (e.g. editor)",
            "search terms", false);
    cl.add("debug", 'd', "turn on the debug output", "", false);
    cl.add("file", 'f', "file or directory", "file", false);

    err = cl.parse();
    if (!err.isEmpty()) {
        WPMUtils::outputTextConsole("Error: " + err + "\n");
        return 1;
    }
    // cl.dump();

    if (cl.isPresent("debug")) {
        clp.setUpdateRate(0);
    }

    QStringList fr = cl.getFreeArguments();

    int r = 0;
    if (fr.count() == 0) {
        WPMUtils::outputTextConsole("Missing command. Try npackdcl help\n",
                false);
        r = 1;
    } else if (fr.count() > 1) {
        WPMUtils::outputTextConsole("Unexpected argument: " +
                fr.at(1) + "\n", false);
        r = 1;
    } else {
        const QString cmd = fr.at(0);
        QString err;
        if (cmd == "help") {
            usage();
        } else if (cmd == "path") {
            err = path();
        } else if (cmd == "remove" || cmd == "rm") {
            err = remove();
        } else if (cmd == "add") {
            err = add();
        } else if (cmd == "add-repo") {
            err = addRepo();
        } else if (cmd == "remove-repo") {
            err = removeRepo();
        } else if (cmd == "list-repos") {
            err = listRepos();
        } else if (cmd == "search") {
            err = search();
        } else if (cmd == "check") {
            err = check();
        } else if (cmd == "which") {
            err = which();
        } else if (cmd == "list") {
            err = list();
        } else if (cmd == "info") {
            err = info();
        } else if (cmd == "update") {
            err = update();
        } else if (cmd == "detect") {
            err = detect();
        } else {
            err = "Wrong command: " + cmd + ". Try npackdcl help";
        }

        if (err.isEmpty())
            r = 0;
        else {
            r = 1;
            WPMUtils::outputTextConsole(err + "\n", false);
        }
    }

    return r;
}

QString App::addNpackdCL()
{
    QString err;

    AbstractRepository* r = AbstractRepository::getDefault_();
    PackageVersion* pv = r->findPackageVersion_(
            "com.googlecode.windows-package-manager.NpackdCL",
            Version(NPACKD_VERSION), &err);
    if (!pv) {
        pv = new PackageVersion(
                "com.googlecode.windows-package-manager.NpackdCL");
        pv->version = Version(NPACKD_VERSION);
        r->savePackageVersion(pv);
    }
    if (!pv->installed()) {
        err = pv->setPath(WPMUtils::getExeDir());
        if (err.isEmpty())
            err = r->updateNpackdCLEnvVar();
    }
    delete pv;

    return err;
}

void App::usage()
{
    WPMUtils::outputTextConsole(QString(
            "NpackdCL %1 - Npackd command line tool\n").
            arg(NPACKD_VERSION));
    const char* lines[] = {
        "Usage:",
        "    npackdcl help",
        "        prints this help",
        "    npackdcl add (--package=<package> [--version=<version>])+",
        "        installs packages. The newest available version will be installed, ",
        "        if none is specified.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    npackdcl remove|rm (--package=<package> [--version=<version>])+",
        "        removes packages. The version number may be omitted, ",
        "        if only one is installed.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    npackdcl update (--package=<package>)+",
        "        updates packages by uninstalling the currently installed",
        "        and installing the newest version. ",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    npackdcl list [--status=installed | all] [--bare-format]",
        "        lists package versions sorted by package name and version.",
        "        Please note that since 1.18 only installed package versions",
        "        are listed regardless of the --status switch.",
        "    npackdcl search [--query=<search terms>] [--status=installed | all]",
        "            [--bare-format]",
        "        full text search. Lists found packages sorted by package name.",
        "        All packages are shown by default.",
        "    npackdcl info --package=<package> [--version=<version>]",
        "        shows information about the specified package or package version",
        "    npackdcl path --package=<package> [--versions=<versions>]",
        "        searches for an installed package and prints its location",
        "    npackdcl add-repo --url=<repository>",
        "        appends a repository to the list",
        "    npackdcl remove-repo --url=<repository>",
        "        removes a repository from the list",
        "    npackdcl list-repos",
        "        list currently defined repositories",
        "    npackdcl detect",
        "        detect packages from the MSI database and software control panel",
        "    npackdcl check",
        "        checks the installed packages for missing dependencies",
        "    npackdcl which --file=<file>",
        "        finds the package that owns the specified file or directory",
        "Options:",
    };
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        WPMUtils::outputTextConsole(QString(lines[i]) + "\n");
    }
    this->cl.printOptions();
    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occures.",
        "If the output is redirected, the texts will be encoded as UTF-8.",
    };    
    for (int i = 0; i < (int) (sizeof(lines2) / sizeof(lines2[0])); i++) {
        WPMUtils::outputTextConsole(QString(lines2[i]) + "\n");
    }
}

QString App::listRepos()
{
    QString err;

    QList<QUrl*> urls = AbstractRepository::getRepositoryURLs(&err);
    if (err.isEmpty()) {
        WPMUtils::outputTextConsole(QString("%1 repositories are defined:\n\n").
                arg(urls.size()));
        for (int i = 0; i < urls.size(); i++) {
            WPMUtils::outputTextConsole(urls.at(i)->toString() + "\n");
        }
    }
    qDeleteAll(urls);

    return err;
}

QString App::which()
{
    QString r;

    InstalledPackages* ip = InstalledPackages::getDefault();
    r = ip->readRegistryDatabase();

    if (r.isEmpty()) {
        Job* job = new Job();

        // ignoring the error as this should be available for non-admins
        ip->refresh(job);

        delete job;
    }

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
            WPMUtils::outputTextConsole(QString(
                    "%1 %2 (%3) is installed in \"%4\"\n").
                    arg(title).arg(f->version.getVersionString()).
                    arg(f->package).arg(f->directory));
            delete p;
            delete f;
        } else
            WPMUtils::outputTextConsole(QString("No package found for \"%1\"\n").
                    arg(file));
    }

    return r;
}

QString App::check()
{
    QString r;

    Job* job = new Job();
    InstalledPackages::getDefault()->refresh(job);
    if (!job->getErrorMessage().isEmpty()) {
        r = job->getErrorMessage();
    }
    delete job;

    if (!r.isEmpty()) {
        WPMUtils::outputTextConsole(
                "Error updating the list of installed packages: " + r + "\n",
                false);
        r = "";
    }

    AbstractRepository* rep = AbstractRepository::getDefault_();
    QList<PackageVersion*> list;

    if (r.isEmpty()) {
        list = rep->getInstalled_(&r);
    }

    if (r.isEmpty()) {
        qSort(list.begin(), list.end(), packageVersionLessThan);

        int n = 0;
        for (int i = 0; i < list.count(); i++) {
            PackageVersion* pv = list.at(i);
            for (int j = 0; j < pv->dependencies.count(); j++) {
                Dependency* d = pv->dependencies.at(j);
                if (!d->isInstalled()) {
                    WPMUtils::outputTextConsole(QString(
                            "%1 depends on %2, which is not installed\n").
                            arg(pv->toString(true)).
                            arg(d->toString(true)));
                    n++;
                }
            }
        }

        if (n == 0)
            WPMUtils::outputTextConsole("All dependencies are installed\n");

        qDeleteAll(list);
    }

    return r;
}

QString App::addRepo()
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
            if (found >= 0) {
                WPMUtils::outputTextConsole(
                        "This repository is already registered: " + url + "\n");
            } else {
                urls.append(url_);
                url_ = 0;
                AbstractRepository::setRepositoryURLs(urls, &err);
                if (err.isEmpty())
                    WPMUtils::outputTextConsole("The repository was added successfully\n");
            }
        }
        qDeleteAll(urls);
    }

    delete url_;

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

    // ignoring the error as "list" should also be usable for non-admins
    InstalledPackages::getDefault()->refresh(job);

    delete job;

    QList<PackageVersion*> list;
    if (err.isEmpty()) {
        AbstractRepository* rep = AbstractRepository::getDefault_();
        list = rep->getInstalled_(&err);
        qSort(list.begin(), list.end(), packageVersionLessThan);
    }

    if (err.isEmpty()) {
        if (!bare)
            WPMUtils::outputTextConsole(QString("%1 package versions found:\n\n").
                    arg(list.count()));

        for (int i = 0; i < list.count(); i++) {
            PackageVersion* pv = list.at(i);
            if (!bare)
                WPMUtils::outputTextConsole(pv->toString() +
                        " (" + pv->package + ")\n");
            else
                WPMUtils::outputTextConsole(pv->package + " " +
                        pv->version.getVersionString() + " " +
                        pv->getPackageTitle() + "\n");
        }
    }

    qDeleteAll(list);

    return err;
}

QString App::search()
{
    bool bare = cl.isPresent("bare-format");
    QString query = cl.get("query");

    Job* job = new Job();

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

    if (job->shouldProceed("Detecting installed software")) {
        Job* rjob = job->newSubJob(0.99);

        // ignoring the error message as this should also be available for
        // non-admins
        InstalledPackages::getDefault()->refresh(rjob);

        delete rjob;
    }

    QList<Package*> list;
    if (job->shouldProceed("Searching for packages")) {
        QString err;
        list = rep->findPackages(Package::INSTALLED, onlyInstalled,
                query, -1, -1, &err);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        qSort(list.begin(), list.end(), packageLessThan);

        if (!bare)
            WPMUtils::outputTextConsole(QString("%1 packages found:\n\n").
                    arg(list.count()));

        for (int i = 0; i < list.count(); i++) {
            Package* p = list.at(i);
            if (!bare)
                WPMUtils::outputTextConsole(p->title +
                        " (" + p->name + ")\n");
            else
                WPMUtils::outputTextConsole(p->name + " " +
                        p->title + "\n");
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
                WPMUtils::outputTextConsole(
                        "The repository was not in the list: " +
                        url + "\n");
            } else {
                delete urls.takeAt(found);
                AbstractRepository::setRepositoryURLs(urls, &err);
                if (err.isEmpty())
                    WPMUtils::outputTextConsole(
                            "The repository was removed successfully\n");
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

    Package* p = 0;

    if (job->shouldProceed()) {
        QString err;
        p = findOnePackage(package, &err);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else if (!p)
            job->setErrorMessage(QString("Unknown package: %1").arg(package));
    }

    Dependency d;
    if (job->shouldProceed()) {
        // debug: WPMUtils::outputTextConsole <<  package) << " " << versions);
        d.package = p->name;
        if (versions.isNull()) {
            d.min.setVersion(0, 0);
            d.max.setVersion(std::numeric_limits<int>::max(), 0);
        } else {
            if (!d.setVersions(versions)) {
                job->setErrorMessage("Cannot parse versions: " +
                        versions);
            }
        }
    }

    if (job->shouldProceed()) {
        // no long-running operation can be done here.
        // "npackdcl path" must be fast.
        QString p = InstalledPackages::getDefault()->findPath_npackdcl(d);
        if (!p.isEmpty()) {
            p.replace('/', '\\');
            WPMUtils::outputTextConsole(p + "\n");
        }
    }
    job->complete();

    QString r = job->getErrorMessage();

    delete p;
    delete job;

    return r;
}

QString App::update()
{
    DBRepository* rep = DBRepository::getDefault();
    Job* job = clp.createJob();

    if (job->shouldProceed("Detecting installed software")) {
        Job* rjob = job->newSubJob(0.05);
        InstalledPackages::getDefault()->refresh(rjob);
        if (!rjob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(rjob->getErrorMessage());
        }
        delete rjob;
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

    QList<Package*> toUpdate;

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            QList<Package*> packages;
            if (package.contains('.')) {
                Package* p = rep->findPackage_(package);
                if (p)
                    packages.append(p);
            } else {
                packages = rep->findPackagesByShortName(package);
            }

            if (job->shouldProceed()) {
                if (packages.count() == 0) {
                    job->setErrorMessage("Unknown package: " + package);
                } else if (packages.count() > 1) {
                    job->setErrorMessage("Ambiguous package name");
                } else {
                    toUpdate.append(packages.at(0)->clone());
                }
            }

            qDeleteAll(packages);

            if (!job->shouldProceed())
                break;
        }
    }

    QList<InstallOperation*> ops;
    bool up2date = false;
    if (job->shouldProceed("Planning")) {
        QString err = rep->planUpdates(toUpdate, ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.12);
            up2date = ops.size() == 0;
        }
    }

    if (job->shouldProceed("Checking locked files and directories") && !up2date) {
        QString msg = Repository::checkLockedFilesForUninstall(ops);
        if (!msg.isEmpty())
            job->setErrorMessage(msg);
        else
            job->setProgress(0.14);
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

    if (job->shouldProceed("Updating") && !up2date) {
        Job* ijob = job->newSubJob(0.86);
        rep->process(ijob, ops);
        if (!ijob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(QString("Error updating: %1").
                    arg(ijob->getErrorMessage()));
        }
        delete ijob;
    }
    qDeleteAll(ops);

    job->complete();

    QString r = job->getErrorMessage();
    if (job->isCancelled()) {
        r = "The package update was cancelled";
    } else if (up2date) {
        WPMUtils::outputTextConsole("The packages are already up-to-date\n");
    } else if (r.isEmpty()) {
        WPMUtils::outputTextConsole("The packages were updated successfully\n");
    }

    delete job;

    qDeleteAll(toUpdate);

    return r;
}

Package* App::findOnePackage(const QString& package, QString* err)
{
    Package* p = 0;

    AbstractRepository* rep = AbstractRepository::getDefault_();
    if (package.contains('.')) {
        p = rep->findPackage_(package);
        if (!p) {
            *err = "Unknown package: " + package;
        }
    } else {
        QList<Package*> packages = rep->findPackagesByShortName(package);

        if (packages.count() == 0) {
            *err = "Unknown package: " + package;
        } else if (packages.count() > 1) {
            QString names;
            for (int i = 0; i < packages.count(); ++i) {
                if (i != 0)
                    names.append(", ");
                Package* pi = packages.at(i);
                names.append(pi->title).append(" (").append(pi->name).
                        append(")");
            }
            *err = QString("Move than one package was found: %1").arg(names);
            qDeleteAll(packages);
        } else {
            p = packages.at(0);
        }
    }

    return p;
}

QList<PackageVersion*> App::getPackageVersionOptions(const CommandLine& cl,
        QString* err, bool add)
{
    QList<PackageVersion*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    AbstractRepository* rep = AbstractRepository::getDefault_();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt->nameMathes("package")) {
            CommandLine::ParsedOption* ponext = 0;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = "Invalid package name: " + package;
            }

            Package* p = 0;
            if (err->isEmpty()) {
                p = findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QString("Unknown package: %1").arg(package);
                }
            }

            PackageVersion* pv = 0;
            if (err->isEmpty()) {
                QString version;
                if (ponext != 0 && ponext->opt->nameMathes("version"))
                    version = ponext->value;
                if (version.isNull()) {
                    if (add) {
                        pv = rep->findNewestInstallablePackageVersion_(
                                p->name, err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QString("No installable version was found for the package %1 (%2)").
                                        arg(p->title).arg(p->name);
                            }
                        }
                    } else {
                        QList<InstalledPackageVersion*> ipvs =
                                InstalledPackages::getDefault()->getByPackage(p->name);
                        if (ipvs.count() == 0) {
                            *err = QString(
                                    "Package %1 (%2) is not installed").
                                    arg(p->title).arg(p->name);
                        } else if (ipvs.count() > 1) {
                            QString vns;
                            for (int i = 0; i < ipvs.count(); i++) {
                                InstalledPackageVersion* ipv = ipvs.at(i);
                                if (!vns.isEmpty())
                                    vns.append(", ");
                                vns.append(ipv->version.getVersionString());
                            }
                            *err = QString(
                                    "More than one version of the package %1 (%2) "
                                    "is installed: %3").arg(p->title).arg(p->name).
                                    arg(vns);
                        } else {
                            pv = rep->findPackageVersion_(p->name,
                                    ipvs.at(0)->version, err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QString("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).arg(version);
                                }
                            }
                        }
                        qDeleteAll(ipvs);
                    }
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = "Cannot parse version: " + version;
                    } else {
                        pv = rep->findPackageVersion_(p->name, version,
                                err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QString("Package version not found: %1 (%2) %3").
                                        arg(p->title).arg(p->name).arg(version);
                            }
                        }
                    }
                }
            }

            if (err->isEmpty()) {
                if (add) {
                    if (pv->installed()) {
                        WPMUtils::outputTextConsole(QString(
                                "%1 is already installed in %2").
                                arg(pv->toString()).
                                arg(pv->getPath()) + "\n");
                    }
                } else {
                    if (!pv->installed()) {
                        WPMUtils::outputTextConsole(QString(
                                "%1 is not installed").
                                arg(pv->toString()) + "\n");
                    }
                }
            }

            if (pv)
                ret.append(pv);

            delete p;
        }
    }

    return ret;
}

QString App::add()
{
    Job* job = clp.createJob();

    if (job->shouldProceed("Detecting installed software")) {
        Job* rjob = job->newSubJob(0.1);
        InstalledPackages::getDefault()->refresh(rjob);
        if (!rjob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(rjob->getErrorMessage());
        }
        delete rjob;
    }

    QString err;
    QList<PackageVersion*> toInstall = getPackageVersionOptions(cl, &err, true);
    if (!err.isEmpty())
        job->setErrorMessage(err);

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
                err = pv->planInstallation(installed, ops, avoid);
            if (!err.isEmpty()) {
                job->setErrorMessage(err);
            }
        }
        qDeleteAll(installed);
    }

    // debug: WPMUtils::outputTextConsole(QString("%1\n").arg(ops.size()));

    AbstractRepository* rep = AbstractRepository::getDefault_();
    if (job->shouldProceed("Installing") && ops.size() > 0) {
        Job* ijob = job->newSubJob(0.9);
        rep->process(ijob, ops);
        if (!ijob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error installing: %1").
                    arg(ijob->getErrorMessage()));

        delete ijob;
    }

    job->complete();

    QString r = job->getErrorMessage();
    if (r.isEmpty()) {
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            WPMUtils::outputTextConsole(QString(
                    "The package %1 was installed successfully in %2\n").arg(
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

    DBRepository* rep = DBRepository::getDefault();

    if (job->shouldProceed("Detecting installed software")) {
        Job* rjob = job->newSubJob(0.1);
        InstalledPackages::getDefault()->refresh(rjob);
        if (!rjob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(rjob->getErrorMessage());
        }
        delete rjob;
    }

    QString err;
    QList<PackageVersion*> toRemove = getPackageVersionOptions(cl, &err, false);
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

    if (job->shouldProceed("Checking locked files and directories")) {
        QString msg = Repository::checkLockedFilesForUninstall(ops);
        if (!msg.isEmpty())
            job->setErrorMessage(msg);
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

    if (job->shouldProceed("Removing")) {
        Job* removeJob = job->newSubJob(0.9);
        rep->process(removeJob, ops);
        if (!removeJob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error removing: %1\n").
                    arg(removeJob->getErrorMessage()));
        delete removeJob;
    }

    job->complete();

    QString r = job->getErrorMessage();
    if (job->isCancelled()) {
        r = "The package removal was cancelled";
    } else if (r.isEmpty()) {
        WPMUtils::outputTextConsole("The packages were removed successfully\n");
    }

    delete job;

    qDeleteAll(ops);
    qDeleteAll(toRemove);

    return r;
}

QString App::info()
{
    QString r;

    Job* job = new Job();

    // ignore the error as "npackdcl info" should be available for non-admins
    // too
    InstalledPackages::getDefault()->refresh(job);

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
        p = this->findOnePackage(package, &r);
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
        WPMUtils::outputTextConsole("Title: " +
                p->title + "\n");
        if (pv)
            WPMUtils::outputTextConsole("Version: " +
                    pv->version.getVersionString() + "\n");
        WPMUtils::outputTextConsole("Description: " + p->description + "\n");
        WPMUtils::outputTextConsole("License: " + p->license + "\n");
        if (pv) {
            WPMUtils::outputTextConsole("Installation path: " +
                    pv->getPath() + "\n");

            InstalledPackages* ip = InstalledPackages::getDefault();
            InstalledPackageVersion* ipv = ip->find(pv->package, pv->version);
            WPMUtils::outputTextConsole("Detection info: " +
                    (ipv ? ipv->detectionInfo : "") + "\n");
        }
        WPMUtils::outputTextConsole("Internal package name: " +
                p->name + "\n");
        if (pv) {
            WPMUtils::outputTextConsole("Status: " +
                    pv->getStatus() + "\n");
            WPMUtils::outputTextConsole("Download URL: " +
                    pv->download.toString() + "\n");
        }
        WPMUtils::outputTextConsole("Package home page: " + p->url + "\n");
        WPMUtils::outputTextConsole("Categories: " +
                p->categories.join(", ") + "\n");
        WPMUtils::outputTextConsole("Icon: " + p->icon + "\n");
        if (pv) {
            WPMUtils::outputTextConsole(QString("Type: ") +
                    (pv->type == 0 ? "zip" : "one-file") + "\n");
            WPMUtils::outputTextConsole("SHA1: " + pv->sha1 + "\n");

            QString details;
            for (int i = 0; i < pv->importantFiles.count(); i++) {
                if (i != 0)
                    details.append("; ");
                details.append(pv->importantFilesTitles.at(i));
                details.append(" (");
                details.append(pv->importantFiles.at(i));
                details.append(")");
            }
            WPMUtils::outputTextConsole("Important files: " + details + "\n");
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
            WPMUtils::outputTextConsole("Versions: " + versions + "\n");
        }

        if (!pv) {
            InstalledPackages* ip = InstalledPackages::getDefault();
            QList<InstalledPackageVersion*> ipvs = ip->getByPackage(p->name);
            if (ipvs.count() > 0) {
                WPMUtils::outputTextConsole(QString("%1 versions are installed:\n").
                        arg(ipvs.count()));
                for (int i = 0; i < ipvs.count(); ++i) {
                    InstalledPackageVersion* ipv = ipvs.at(i);
                    if (!ipv->getDirectory().isEmpty())
                        WPMUtils::outputTextConsole("    " +
                                ipv->version.getVersionString() +
                                " in " + ipv->getDirectory() + "\n");
                }
            } else {
                WPMUtils::outputTextConsole("No versions are installed\n");
            }
            qDeleteAll(ipvs);
        }
    }


    if (r.isEmpty()) {
        if (pv) {
            WPMUtils::outputTextConsole("Dependency tree:\n");
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

        WPMUtils::outputTextConsole(parentPrefix + prefix + before + s + "\n");

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
    job->setHint("Loading repositories and detecting installed software");

    DBRepository* rep = DBRepository::getDefault();
    rep->updateF5(job);
    QString r = job->getErrorMessage();
    if (job->getErrorMessage().isEmpty()) {
        WPMUtils::outputTextConsole("Package detection completed successfully\n");
    }
    delete job;

    return r;
}
