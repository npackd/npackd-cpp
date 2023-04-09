#include "abstractthirdpartypm.h"

AbstractThirdPartyPM::AbstractThirdPartyPM()
{
}

AbstractThirdPartyPM::~AbstractThirdPartyPM()
{
}

void AbstractThirdPartyPM::scanRunnable(Job *job,
        std::vector<InstalledPackageVersion *> *installed, Repository *rep)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    CoInitialize(nullptr);
    scan(job, installed, rep);
    CoUninitialize();
}
