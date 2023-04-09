#ifndef JOB_H
#define JOB_H

#include <windows.h>
#include <mutex>

#include <QString>
#include <QObject>
#include <QMetaType>
#include <QTime>

class Job;

/**
 * A long-running task.
 *
 * A task is typically defined as a function with the following signature:
 *     void longRunning(Job* job)
 *
 * The implementation follows this pattern:
 * {
 *     for (int i = 0; i < 100; i++) {
 *        if (job->isCancelled())
 *            break;
 *        job->setTitle(QString("Processing step %1").arg(i));
 *        .... do some work here
 *        job->setProgress(((double) i) / 100);
 *     }
 *     job->complete();
 * }
 *
 * The function is called like this:
 * Job* job = new Job(); // or job = mainJob->createSubJob();
 * longRunning(job);
 * if (job->isCancelled()) {
 *     ....
 * } else if (!job->getErrorMessage().isEmpty()) {
 *    ....
 * } else {
 *     ....
 * }
 */
class Job: public QObject
{
    Q_OBJECT
private:
    mutable std::recursive_mutex mutex;

    /** timeout in seconds or 0 or "unlimited" */
    int timeout = 0;

    std::vector<Job*> childJobs;

    /** progress 0...1 */
    double progress;

    QString title;

    QString errorMessage;

    double subJobSteps;
    double subJobStart;

    /** true if the user presses the "cancel" button. */
    bool cancelRequested;

    bool completed;

    /** time when this job was started or 0 */
    time_t started;

    /** should the parent progress be updated? */
    bool uparentProgress;

    bool updateParentErrorMessage;

    /**
     * @threadsafe
     */
    void updateParentProgress();

    /**
     * @threadsafe
     */
    void fireChange();

    void fireSubJobCreated(Job *sub);

    void fireChange(Job *s);
public slots:
    /**
     * @threadsafe
     */
    void parentJobChanged(Job *s);
public:
    /** parent job or 0 */
    Job* parentJob;

    /**
     * @brief creates a new job
     * @param title title for this job
     * @param parent parent job or 0
     */
    Job(const QString& title="", Job* parent=nullptr);

    ~Job();

    /**
     * @threadsafe
     * @return true if this job was completed: with or without an error. If a
     *     user cancells this job and it is not yet completed it means that the
     *     cancelling is in progress.
     */
    bool isCompleted();

    /**
     * This must be called in order to complete the job regardless of
     * setProgress, errors or cancellation state.
     *
     * This method does not wait to child jobs.
     *
     * @threadsafe
     */
    void complete();

    /**
     * @brief sets the progress to 1 and completes the job
     * @threadsafe
     */
    void completeWithProgress();

    /**
     * Request cancelling of this job.
     *
     * @threadsafe
     */
    void cancel();

    /**
     * @return true if this job was cancelled.
     * @threadsafe
     */
    bool isCancelled() const;

    /**
     * Creates a sub-job for the specified part of this job. The error message
     * from the sub-job does not automatically propagate to the parent job.
     *
     * @param part 0..1 part of this for the created sub-job
     * @param title title for the new job
     * @param updateParentProgress_ true = update progress of the parent job
     * @param updateParentErrorMessage true = update the error message of the
     *     parent job
     * @return child job with parent=this. The created job is still owned by
     *     this object and will be destroyed automatically.
     * @threadsafe
     */
    Job* newSubJob(double part, const QString& title="",
            bool updateParentProgress_=true,
            bool updateParentErrorMessage=false);

    /**
     * @return progress of this job (0...1)
     * @threadsafe
     */
    double getProgress() const;

    /**
     * @return level (0..)
     * @threadsafe
     */
    int getLevel() const;

    /**
     * @return top level job
     * @threadsafe
     */
    const Job* getTopJob() const;

    /**
     * @return the title
     * @threadsafe
     */
    QString getTitle() const;

    /**
     * @return the "title 1 / title 2 / ..."
     * @threadsafe
     */
    QString getFullTitle() const;

    /**
     * @brief sets the timeout
     * @param timeout timeout in seconds or 0 for "unlimited"
     */
    void setTimeout(int timeout);

    /**
     * Sets the progress. The value is only informational. You have to
     * call complete() at the end anyway.
     *
     * @param progress new progress (0...1)
     * @threadsafe
     */
    void setProgress(double progress);

    /**
     * @return error message. If the error message is not empty, the
     *     job ended with an error.
     * @threadsafe
     */
    QString getErrorMessage() const;

    /**
     * Sets an error message for this job. The error message only propagates
     * to the parent job if updateParentErrorMessage is true.
     *
     * @param errorMessage new error message
     * @threadsafe
     */
    void setErrorMessage(const QString &errorMessage);

    /**
     * This function can be used to simplify the following case:
     *
     * if (!job->isCancelled() && job->getErrorMessage().isEmpty() {
     *     ...
     * }
     *
     * as follows:
     *
     * if (job->shouldProceed() {
     *     ...
     * }
     *
     * @return true if this job is neither cancelled nor failed
     * @threadsafe
     */
    bool shouldProceed() const;

    /**
     * @brief changes the title
     * @param title new title
     */
    void setTitle(const QString &title);

    /**
     * @brief checks the value returned by a Windows API function and sets
     *     the error message to the formatted value of GetLastError() if the
     *     value is false. The error message will not be changed if it is
     *     already non-empty.
     * @param v the value returned by a Windows API function like
     *     SetHandleInformation
     */
    void checkOSCall(bool v);

    /**
     * @brief checks the value returned by a Windows API function and sets
     *     the error message if necessary. The error message will not be changed if it is
     *     already non-empty.
     * @param v the value returned by a Windows API function like
     *     CoCreateInstance
     */
    void checkHResult(HRESULT v);

    /**
     * @return the approximate time necessary to complete the rest of this task
     */
    time_t remainingTime();

    /**
     * @return start time for this job
     */
    time_t getStarted();

    /**
     * @brief waitFor waits till this job and all its children are completed
     */
    void waitFor();

    /**
     * @brief waitFor waits till all children are completed
     */
    void waitForChildren();

    /**
     * @brief checks the timeout and cancells this job, if necessary. This call
     *     also propagates to the parent job.
     */
    void checkTimeout();
signals:
    /**
     * This signal will be fired each time something in this object
     * changes (progress, hint etc.).
     */
    void changed(Job* s);

    /**
     * Job was completed with an error message or successfully. This signal is
     * only emitted once and only on this task (child jobs are not considered).
     */
    void jobCompleted();

    /**
     * @brief a new sub-job was created
     * @param sub new sub-job
     */
    void subJobCreated(Job* sub);
};

#endif // JOB_H
