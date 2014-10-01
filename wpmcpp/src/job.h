#ifndef JOB_H
#define JOB_H

#include <QString>
#include <QObject>
#include <QMetaType>
#include <QMutex>
#include <QQueue>
#include <QTime>
#include <QList>

class Job;

/**
 * This class is used to exchange data about a Job between threads.
 */
class JobState: public QObject {
    Q_OBJECT
public:
    /** job progress: 0..1 */
    double progress;

    /** title */
    QString title;

    /** job hint */
    QString hint;

    /** job error message or "" */
    QString errorMessage;

    /** true, if the job was cancelled by the user */
    bool cancelRequested;

    /** the job was completed (with or without an error) */
    bool completed;

    /** the job */
    Job* job;

    /** time of the start or 0 */
    time_t started;

    JobState();
    JobState(const JobState& s);
    void operator=(const JobState& s);

    /**
     * @return time remaining time necessary to complete this task
     */
    time_t remainingTime();
};

Q_DECLARE_METATYPE(JobState)

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
 *        job->setHint(QString("Processing step %1").arg(i));
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
    mutable QMutex mutex;

    QList<Job*> childJobs;

    /** progress 0...1 */
    double progress;

    /** description of the current state */
    QString hint;

    QString title;

    QString errorMessage;

    QString parentHintStart;
    double subJobSteps;
    double subJobStart;

    /** true if the user presses the "cancel" button. */
    bool cancelRequested;

    bool completed;

    /** time when this job was started or 0 */
    time_t started;

    /** should the parent hint be updated? */
    bool uparentHint;

    /** should the parent progress be updated? */
    bool uparentProgress;

    /**
     * @threadsafe
     */
    void updateParentHint();

    /**
     * @threadsafe
     */
    void updateParentProgress();

    /**
     * @threadsafe
     */
    void fireChange();

    void fireSubJobCreated(Job *sub);

    void fireChange(const JobState &s);
public slots:
    /**
     * @threadsafe
     */
    void parentJobChanged(const JobState& s);
public:
    /** currently running jobs */
    static QList<Job*> jobs;

    /** parent job or 0 */
    Job* parentJob;

    /**
     * @brief creates a new job
     * @param title title for this job
     * @param parent parent job or 0
     */
    Job(const QString& title="", Job* parent=0);

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
     * @threadsafe
     */
    void complete();

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
     * @param updateParentHint_ true = update hint of the parent job
     * @param updateParentProgress_ true = update progress of the parent job
     * @return child job with parent=this
     * @threadsafe
     */
    Job* newSubJob(double part, const QString& title="",
            bool updateParentHint_=true,
            bool updateParentProgress_=true);

    /**
     * @return progress of this job (0...1)
     * @threadsafe
     */
    double getProgress() const;

    /**
     * @return current hint
     * @threadsafe
     */
    QString getHint() const;

    /**
     * @return the title
     * @threadsafe
     */
    QString getTitle() const;

    /**
     * @param hint new hint
     * @threadsafe
     */
    void setHint(const QString& hint);

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
     * Sets an error message for this job. The error message does *not*
     * automatically propagate to the parent job.
     *
     * @param errorMessage new error message
     * @threadsafe
     */
    void setErrorMessage(const QString &errorMessage);

    /**
     * This function can be used to simplify the following case:
     *
     * if (!job->isCancelled() && job->getErrorMessage().isEmpty() {
     *     job.setHint("Testing");
     *     ...
     * }
     *
     * as follows:
     *
     * if (job->shouldProceed("Testing") {
     *     ...
     * }
     *
     * @param hint new hint
     * @return true if this job is neither cancelled nor failed
     * @threadsafe
     */
    bool shouldProceed(const QString& hint);

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
signals:
    /**
     * This signal will be fired each time something in this object
     * changes (progress, hint etc.).
     */
    void changed(const JobState& s);

    /**
     * @brief a new sub-job was created
     * @param sub new sub-job
     */
    void subJobCreated(Job* sub);
};

#endif // JOB_H
