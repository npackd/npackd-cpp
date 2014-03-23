#include "scanharddrivesthread.h"
#include "job.h"
#include "dbrepository.h"
#include "scandiskthirdpartypm.h"
#include "installedpackages.h"

ScanHardDrivesThread::ScanHardDrivesThread(Job *job)
{
    this->job = job;
}

void ScanHardDrivesThread::run()
{
    /* creating a tag cloud
    QString err;
    QList<Package*> ps = DBRepository::getDefault()->findPackages(
            Package::INSTALLED, false, "", &err);
    words.clear();
    QRegExp re("\\W+", Qt::CaseInsensitive);
    for (int i = 0; i < ps.count(); i++) {
        Package* p = ps.at(i);
        QString txt = p->title + " " + p->description;
        QStringList sl = txt.toLower().split(re, QString::SkipEmptyParts);
        sl.removeDuplicates();
        for (int j = 0; j < sl.count(); j++) {
            QString w = sl.at(j);
            if (w.length() > 3) {
                int n = words.value(w);
                n++;
                words.insert(w, n);
            }
        }
    }
    qDeleteAll(ps);

    QStringList stopWords = QString("a an and are as at be but by for if in "
            "into is it no not of on or such that the their then there these "
            "they this to was will with").split(" ");
    for (int i = 0; i < stopWords.count(); i++)
        words.remove(stopWords.at(i));
        */

    QString err;
    DBRepository* r = DBRepository::getDefault();
    QList<PackageVersion*> s1 = r->getInstalled_(&err);
    if (!err.isEmpty())
        job->setErrorMessage(err);


    ScanDiskThirdPartyPM* sd = new ScanDiskThirdPartyPM();
    err = InstalledPackages::getDefault()->detect3rdParty(r, sd, false);
    delete sd;
    if (!err.isEmpty())
        job->setErrorMessage(err);

    QList<PackageVersion*> s2 = r->getInstalled_(&err);
    if (!err.isEmpty())
        job->setErrorMessage(err);

    for (int i = 0; i < s2.count(); i++) {
        PackageVersion* pv = s2.at(i);
        if (!PackageVersion::contains(s1, pv)) {
            detected.append(pv);
        }
    }

    qDeleteAll(s1);
    qDeleteAll(s2);
}

