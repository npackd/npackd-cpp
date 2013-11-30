#include "scandiskthirdpartypm.h"

#include <QDebug>

#include "wpmutils.h"
#include "dbrepository.h"

ScanDiskThirdPartyPM::ScanDiskThirdPartyPM()
{
}

void ScanDiskThirdPartyPM::scan(Job *job,
        QList<InstalledPackageVersion *> *installed, Repository *rep) const
{
    QStringList ignore;
    ignore.append(WPMUtils::normalizePath(WPMUtils::getWindowsDir()));

    QFileInfoList fil = QDir::drives();
    for (int i = 0; i < fil.count(); i++) {
        if (job->isCancelled())
            break;

        QFileInfo fi = fil.at(i);

        job->setHint(QString(QObject::tr("Scanning %1")).arg(fi.absolutePath()));
        Job* djob = job->newSubJob(1.0 / fil.count());
        QString path = WPMUtils::normalizePath(fi.absolutePath());
        UINT t = GetDriveType((WCHAR*) path.utf16());
        if (t == DRIVE_FIXED)
            scan(path, djob, 0, ignore);
        delete djob;
    }

    job->complete();
}

void ScanDiskThirdPartyPM::scan(const QString& path, Job* job, int level,
        QStringList& ignore) const
{
    if (ignore.contains(path))
        return;

    QDir aDir(path);

    QMap<QString, QString> path2sha1;

    DBRepository* r = DBRepository::getDefault();
    QString err;
    QList<PackageVersion*> packageVersions =
            r->getPackageVersionsWithDetectFiles(&err);

    // qDebug() << "package versions with detect files: " << packageVersions.count();

    if (!err.isEmpty())
        job->setErrorMessage(err);

    for (int i = 0; i < packageVersions.count(); i++) {
        if (!job->shouldProceed())
            break;

        PackageVersion* pv = packageVersions.at(i);
        if (!pv->installed() && pv->detectFiles.count() > 0) {
            boolean ok = true;
            for (int j = 0; j < pv->detectFiles.count(); j++) {
                bool fileOK = false;
                DetectFile* df = pv->detectFiles.at(j);
                if (aDir.exists(df->path)) {
                    QString fullPath = path + "\\" + df->path;
                    QFileInfo f(fullPath);
                    if (f.isFile() && f.isReadable()) {
                        QString sha1 = path2sha1.value(df->path);
                        if (sha1.isEmpty()) {
                            sha1 = WPMUtils::sha1(fullPath);
                            path2sha1[df->path] = sha1;
                        }
                        if (df->sha1 == sha1) {
                            fileOK = true;
                        }
                    }
                }
                if (!fileOK) {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                pv->setPath(path);
                return;
            }
        }
    }

    if (job && !job->isCancelled()) {
        QFileInfoList entries = aDir.entryInfoList(
                QDir::NoDotAndDotDot | QDir::Dirs);
        int count = entries.size();
        for (int idx = 0; idx < count; idx++) {
            if (job && job->isCancelled())
                break;

            QFileInfo entryInfo = entries[idx];
            QString name = entryInfo.fileName();

            if (job) {
                job->setHint(QString("%1").arg(name));
                if (job->isCancelled())
                    break;
            }

            Job* djob;
            if (level < 2)
                djob = job->newSubJob(1.0 / count);
            else
                djob = 0;
            scan(path + "\\" + name.toLower(), djob, level + 1, ignore);
            delete djob;

            if (job) {
                job->setProgress(((double) idx) / count);
            }
        }
    }

    qDeleteAll(packageVersions);

    if (job)
        job->complete();
}

