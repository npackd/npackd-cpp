#include <windows.h>

#include "installthread.h"
#include "abstractrepository.h"
#include "wpmutils.h"
#include "dbrepository.h"

InstallThread::InstallThread(PackageVersion *pv, int type, Job* job)
{
    this->pv = pv;
    this->type = type;
    this->job = job;
    this->useCache = false;
}

void InstallThread::run()
{
    CoInitialize(NULL);

    // qDebug() << "InstallThread::run.1";
    switch (this->type) {
        case 0:
        case 1:
        case 2:
            AbstractRepository::getDefault_()->process(job, install,
                    WPMUtils::getCloseProcessType());
            break;
        case 3:
        case 4: {
            DBRepository* dbr = DBRepository::getDefault();
            dbr->updateF5(job);
            break;
        }
    }

    CoUninitialize();

    // qDebug() << "InstallThread::run.2";
}

