#include <windows.h>

#include "updaterepositorythread.h"
#include "abstractrepository.h"
#include "wpmutils.h"
#include "dbrepository.h"

UpdateRepositoryThread::UpdateRepositoryThread(Job* job) : job(job)
{
}

void UpdateRepositoryThread::run()
{
    CoInitialize(NULL);

    DBRepository* dbr = DBRepository::getDefault();
    dbr->updateF5(job);

    CoUninitialize();
}

