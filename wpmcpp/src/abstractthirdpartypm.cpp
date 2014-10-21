#include <QThread>

#include "abstractthirdpartypm.h"

AbstractThirdPartyPM::AbstractThirdPartyPM()
{
}

AbstractThirdPartyPM::~AbstractThirdPartyPM()
{
}

void AbstractThirdPartyPM::scanRunnable(Job *job, QList<InstalledPackageVersion *> *installed, Repository *rep)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);
    CoInitialize(0);
    scan(job, installed, rep);
    CoUninitialize();
}
