#ifndef VISIBLEJOBS_H
#define VISIBLEJOBS_H

#include <time.h>

#include <QObject>
#include <QList>

#include "job.h"

/**
 * @brief list of jobst that should be visible to the user.
 */
class VisibleJobs: public QObject
{
    Q_OBJECT
private:
    static VisibleJobs def;

    VisibleJobs();
public:
    time_t monitoredJobLastChanged;
    QList<Job*> runningJobs;
    QList<JobState> runningJobStates;

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
signals:
    /**
     * This signal will be fired each time a job is added or removed
     * from the list.
     */
    void changed();
};

#endif // VISIBLEJOBS_H
