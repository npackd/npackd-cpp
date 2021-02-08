#ifndef VISIBLEJOBS_H
#define VISIBLEJOBS_H

#include <time.h>
#include <mutex>

#include <QObject>

#include "job.h"

/**
 * @brief list of jobst that should be visible to the user.
 */
class VisibleJobs: public QObject
{
    Q_OBJECT
private:
    static VisibleJobs def;

    mutable std::mutex mutex;

    /** running jobs. The access should be guarded by the mutex */
    std::vector<Job*> runningJobs;

    VisibleJobs();
public:
    /**
     * @return default instance
     */
    static VisibleJobs* getDefault();

    /**
     * Unregistered a currently running monitored job.
     *
     * @param job a currently running and monitored job
     */
    void unregisterJob(Job *job);

    /**
     * @return number of currently registered jobs
     */
    size_t size() const;

    /**
     * @brief registeres a job
     * @param job the job
     */
    void push_back(Job* job);

    /**
     * @brief estimates the time to complete all tasks
     * @return remaining time, maximum progress of a task
     */
    std::tuple<time_t, double> getRemainingTime() const;
signals:
    /**
     * This signal will be fired each time a job is added or removed
     * from the list.
     */
    void changed();
};

#endif // VISIBLEJOBS_H
