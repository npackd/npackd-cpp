#include "visiblejobs.h"

VisibleJobs VisibleJobs::def;

VisibleJobs::VisibleJobs()
{
}

VisibleJobs *VisibleJobs::getDefault()
{
    return &def;
}

void VisibleJobs::unregisterJob(Job *job)
{
    {
        std::unique_lock<std::mutex> lock{mutex};

        auto it = std::find(runningJobs.begin(), runningJobs.end(), job);
        if (it != runningJobs.end()) {
            runningJobs.erase(it);
        }
    }

    emit changed();
}

size_t VisibleJobs::size() const
{
    std::unique_lock<std::mutex> lock{mutex};
    return runningJobs.size();
}

void VisibleJobs::push_back(Job *job)
{
    {
        std::unique_lock<std::mutex> lock{mutex};
        runningJobs.push_back(job);
    }

    emit changed();
}

std::tuple<time_t, double> VisibleJobs::getRemainingTime() const
{
    std::unique_lock<std::mutex> lock{mutex};

    time_t max = -1;
    double maxProgress = 0;
    for (auto state: runningJobs) {
        // state.job may be null if the corresponding task was just started
        time_t t = state->remainingTime();
        if (t > max) {
            max = t;
            maxProgress = state->getProgress();
        }
    }
    if (max < 0)
        max = 0;

    return std::tie(max, maxProgress);
}
