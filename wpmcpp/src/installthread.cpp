#include <windows.h>

#include "installthread.h"
#include "abstractrepository.h"
#include "wpmutils.h"
#include "dbrepository.h"

InstallThread::InstallThread(Job* job)
{
    this->job = job;
    this->programCloseType = WPMUtils::getCloseProcessType();
}

void InstallThread::run()
{
    CoInitialize(NULL);

    AbstractRepository::getDefault_()->process(job, install,
            this->programCloseType);

    CoUninitialize();

    // qDebug() << "InstallThread::run.2";
}

