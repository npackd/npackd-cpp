#include "vimorgrepapp.h"

#include <QCoreApplication>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QDomDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#include "wpmutils.h"
#include "downloader.h"
#include "clprogress.h"
#include "repository.h"
#include "package.h"
#include "packageversion.h"
#include "packageversionfile.h"
#include "dependency.h"

VimOrgRepApp::VimOrgRepApp()
{
}

int VimOrgRepApp::process()
{
    int r = 0;

    CLProgress clp;

    Job* job = clp.createJob();

    Job* sub = job->newSubJob(0.8);
    QTemporaryFile* f = Downloader::download(sub,
            QUrl("http://www.vim.org/script-info.php"));
    if (!sub->getErrorMessage().isEmpty())
        job->setErrorMessage(sub->getErrorMessage());
    delete sub;

    QJsonDocument d;
    if (job->shouldProceed()) {
        if (f->open()) {
            QJsonParseError error;
            d = QJsonDocument::fromJson(
                    f->readAll(), &error);
            if (d.isNull())
                job->setErrorMessage(QString("JSON validation failed: %1").
                        arg(error.errorString()));
            else
                job->setProgress(0.85);
        } else {
            job->setErrorMessage("Cannot open the temporary file");
        }
    }
    delete f;

    Repository rep;

    QJsonObject root;
    if (job->shouldProceed("Creating the repository")) {
        if (d.isObject()) {
            root = d.object();
        } else {
            job->setErrorMessage("Not an object at the root of the JSON file");
        }
    }

    if (job->shouldProceed()) {
        QStringList ids = root.keys();

        for (int i = 0; i < ids.size(); i++) {
            QJsonValue jp_ = root.value(ids.at(i));
            if (jp_.isObject()) {
                QJsonObject jp = jp_.toObject();

                // TODO: also use install_details

                QString scriptType = jp.value("script_type").toString();
                QString scriptName = jp.value("script_name").toString();
                QString description = jp.value("description").toString();

                // TODO: replace invalid characters
                QString packageName = scriptName;
                packageName.replace('.', '-');
                packageName = "org.vim." + packageName;

                if (!WPMUtils::validateFullPackageName(packageName).isEmpty())
                    continue;

                Package* p = new Package(packageName, "Vim plugin " + scriptName);
                p->description = description;
                p->categories.append("Text/" + scriptType);
                rep.savePackage(p);
                delete p;

                QJsonArray releases = jp.value("releases").toArray();
                for (int j = 0; j < releases.size(); j++) {
                    QJsonObject release = releases.at(j).toObject();
                    QString scriptVersion = release.value("script_version").
                            toString();
                    QString jpackage = release.value("package").
                            toString();

                    // TODO: process other package types
                    // TODO: not all zip files are in the right format
                    if (jpackage.toLower().endsWith(".zip")) {
                        QString srcId = release.value("src_id").
                                toString();
                        QString vimVersion = release.value("vim_version").
                                toString();

                        PackageVersion* pv = new PackageVersion(packageName);
                        pv->version.setVersion(scriptVersion); // TODO: check error
                        pv->download = "http://www.vim.org/scripts/download_script.php?src_id=" +
                                srcId;

                        Dependency* d = new Dependency();
                        d->package = "com.googlecode.windows-package-manager.NpackdInstallerHelper";
                        d->setVersions("[1.4, 2)");
                        d->var = "nih";
                        pv->dependencies.append(d);

                        d = new Dependency();
                        d->package = "vim-pathogen";
                        d->setVersions("[2.3, 3)");
                        pv->dependencies.append(d);

                        d = new Dependency();
                        d->package = "vim-huge";
                        d->setVersions("[" + vimVersion + ", 8)");
                        pv->dependencies.append(d);

                        PackageVersionFile* pvf = new PackageVersionFile(
                                ".Npackd\\Install.bat",
                                "\"%nih%\\RegisterVimPlugin.bat\"");
                        pv->files.append(pvf);

                        pvf = new PackageVersionFile(
                                ".Npackd\\Uninstall.bat",
                                "\"%nih%\\UnregisterVimPlugin.bat\"");
                        pv->files.append(pvf);

                        pv->type = (jpackage.toLower().endsWith(".zip")) ? 0 : 1;

                        rep.savePackageVersion(pv);
                        delete pv;
                    }
                }
            }
        }

        job->setProgress(0.9);
    }

    if (false && job->shouldProceed("Downloading the binaries")) {
        int n = rep.packageVersions.size();
        for (int i = 0; i < n; i++) {
            PackageVersion* pv = rep.packageVersions.at(i);
            job->setHint(QString("Downloading package %1 of %2").arg(i + 1).
                    arg(n));

            QString s = pv->download.toString();
            QString id = s.right(s.length() - s.lastIndexOf('=') - 1);
            QFile* f = new QFile("files\\" + id);
            if (!f->exists()) {
                if (f->open(QFile::ReadWrite)) {
                    Job* sub = job->newSubJob(0.1 / n);
                    Downloader::download(sub,
                            QUrl(pv->download), f);
                    if (!sub->getErrorMessage().isEmpty()) {
                        job->setErrorMessage(sub->getErrorMessage());
                    }
                    delete sub;
                } else {
                    job->setErrorMessage(QString("Error opening %1").
                            arg(f->fileName()));
                }
            } else {
                job->setProgress(job->getProgress() + 0.1 / n);
            }
            delete f;

            if (!job->shouldProceed())
                break;
        }
    }

    if (job->shouldProceed("Saving the repository")) {
        QString e = rep.writeTo("VimOrgRep.xml");
        if (!e.isEmpty())
            job->setErrorMessage(e);
        else
            job->setProgress(1);
    }

    if (!job->getErrorMessage().isEmpty()) {
        WPMUtils::outputTextConsole(job->getErrorMessage() + "\n", false);
        r = 1;
    }
    delete job;

    QCoreApplication::instance()->exit(r);
    return r;
}
