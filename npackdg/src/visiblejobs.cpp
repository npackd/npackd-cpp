#include "visiblejobs.h"

VisibleJobs VisibleJobs::def;

VisibleJobs::VisibleJobs() : monitoredJobLastChanged(0)
{
}

VisibleJobs *VisibleJobs::getDefault()
{
    return &def;
}

void VisibleJobs::unregisterJob(Job *job)
{
    auto it = std::find(runningJobs.begin(), runningJobs.end(), job);
    if (it != runningJobs.end()) {
        runningJobs.erase(it);
    }

    emit changed();
}
