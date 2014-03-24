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
    int index = runningJobs.indexOf(job);
    if (index >= 0) {
        runningJobs.removeAt(index);
        runningJobStates.removeAt(index);
    }

    emit changed();
}
