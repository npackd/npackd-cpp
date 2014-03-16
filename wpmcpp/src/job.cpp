#include <math.h>
#include <time.h>

#include "qdebug.h"
#include "qmutex.h"

#include "job.h"

JobState::JobState()
{
    this->cancelRequested = false;
    this->completed = false;
    this->errorMessage = "";
    this->hint = "";
    this->progress = 0;
    this->job = 0;
    this->started = 0;
}

JobState::JobState(const JobState& s) : QObject()
{
    this->cancelRequested = s.cancelRequested;
    this->completed = s.completed;
    this->errorMessage = s.errorMessage;
    this->hint = s.hint;
    this->progress = s.progress;
    this->job = s.job;
    this->started = s.started;
}

void JobState::operator=(const JobState& s)
{
    this->cancelRequested = s.cancelRequested;
    this->completed = s.completed;
    this->errorMessage = s.errorMessage;
    this->hint = s.hint;
    this->progress = s.progress;
    this->job = s.job;
    this->started = s.started;
}

time_t JobState::remainingTime()
{
    time_t result;
    if (started != 0) {
        time_t now;
        time(&now);

        time_t diff = difftime(now, started);

        diff = difftime(now, started);
        diff = lround(diff * (1 / this->progress - 1));
        result = diff;
    } else {
        result = 0;
    }
    return result;
}

QList<Job*> Job::jobs;

Job::Job() : mutex(QMutex::Recursive)
{
    this->progress = 0.0;
    this->parentJob = 0;
    this->cancelRequested = false;
    this->completed = false;
    this->started = 0;
}

Job::~Job()
{
    Job* parentJob_;
    this->mutex.lock();
    parentJob_ = this->parentJob;
    this->mutex.unlock();

    if (parentJob_) {
        parentJob_->setHint(parentHintStart);
    }
}

void Job::complete()
{
    bool completed_;
    this->mutex.lock();
    completed_ = this->completed;
    if (!completed_) {
        this->completed = true;
    }
    this->mutex.unlock();

    if (!completed_)
        fireChange();
}

bool Job::isCancelled() const
{
    bool cancelRequested_;
    this->mutex.lock();
    cancelRequested_ = this->cancelRequested;
    this->mutex.unlock();

    return cancelRequested_;
}

void Job::cancel()
{
    bool cancelRequested_;
    Job* parentJob_;
    this->mutex.lock();
    cancelRequested_ = this->cancelRequested;
    parentJob_ = this->parentJob;
    if (!cancelRequested_) {
        this->cancelRequested = true;
    }
    this->mutex.unlock();

    if (!cancelRequested_) {
        if (parentJob_)
            parentJob_->cancel();

        fireChange();
    }
}

void Job::parentJobChanged(const JobState& s)
{
    if (s.cancelRequested && !this->isCancelled())
        this->cancel();
}

Job* Job::newSubJob(double part)
{
    Job* r = new Job();
    r->parentJob = this;
    r->subJobSteps = part;
    r->subJobStart = this->getProgress();
    r->parentHintStart = getHint();

    // bool success =
    connect(this, SIGNAL(changed(const JobState&)),
            r, SLOT(parentJobChanged(const JobState&)),
            Qt::DirectConnection);

    return r;
}

bool Job::isCompleted()
{
    bool completed_;
    this->mutex.lock();
    completed_ = this->completed;
    this->mutex.unlock();
    return completed_;
}

void Job::fireChange()
{
    this->mutex.lock();
    if (this->started == 0)
        time(&this->started);

    JobState state;
    state.job = this;
    state.cancelRequested = this->cancelRequested;
    state.completed = this->completed;
    state.errorMessage = this->errorMessage;
    state.hint = this->hint;
    state.progress = this->progress;
    state.started = this->started;
    this->mutex.unlock();

    emit changed(state);
}

void Job::setProgress(double progress)
{
    this->mutex.lock();
    this->progress = progress;
    this->mutex.unlock();

    fireChange();

    updateParentProgress();
    updateParentHint();
}

void Job::updateParentProgress()
{
    Job* parentJob_;
    double d;
    this->mutex.lock();
    parentJob_ = parentJob;
    if (parentJob) {
        d = this->subJobStart +
                this->getProgress() * this->subJobSteps;
    } else {
        d = 0.0;
    }
    this->mutex.unlock();

    if (parentJob_)
        parentJob_->setProgress(d);
}

double Job::getProgress() const
{
    double progress_;
    this->mutex.lock();
    progress_ = this->progress;
    this->mutex.unlock();

    return progress_;
}

QString Job::getHint() const
{
    QString hint_;
    this->mutex.lock();
    hint_ = this->hint;
    this->mutex.unlock();

    return hint_;
}

void Job::setHint(const QString &hint)
{
    this->mutex.lock();
    this->hint = hint;
    // qDebug() << hint;
    this->mutex.unlock();

    fireChange();

    updateParentHint();
}

void Job::updateParentHint()
{
    Job* parentJob_;
    QString hint_;
    QString parentHintStart_;
    this->mutex.lock();
    parentJob_ = this->parentJob;
    hint_ = this->hint;
    parentHintStart_ = this->parentHintStart;
    this->mutex.unlock();

    if (parentJob_) {
        if ((isCompleted() && getErrorMessage().isEmpty())
                || hint_.isEmpty())
            parentJob_->setHint(parentHintStart_);
        else
            parentJob_->setHint(parentHintStart_ + " / " + hint_);
    }
}

QString Job::getErrorMessage() const
{
    QString errorMessage_;
    this->mutex.lock();
    errorMessage_ = this->errorMessage;
    this->mutex.unlock();

    return errorMessage_;
}

bool Job::shouldProceed(const QString& hint)
{
    bool b = !this->isCancelled() && this->getErrorMessage().isEmpty();
    if (b)
        setHint(hint);
    return b;
}

bool Job::shouldProceed() const
{
    bool b = !this->isCancelled() && this->getErrorMessage().isEmpty();
    return b;
}

void Job::setErrorMessage(const QString &errorMessage)
{
    this->mutex.lock();
    this->errorMessage = errorMessage;
    this->mutex.unlock();
    fireChange();
}
