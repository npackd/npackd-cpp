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

Job::Job(const QString &title, Job *parent): mutex(QMutex::Recursive), parentJob(parent)
{
    this->title = title;
    this->progress = 0.0;
    this->subJobStart = 0;
    this->subJobSteps = -1;
    this->cancelRequested = false;
    this->completed = false;
    this->started = 0;
    this->uparentHint = true;
    this->uparentProgress = true;
}

Job::~Job()
{
    Job* parentJob_;
    this->mutex.lock();
    parentJob_ = parentJob;
    this->mutex.unlock();

    if (parentJob_) {
        parentJob_->setHint(parentHintStart);
    }

    for (int i = 0; i < this->childJobs.count(); i++) {
        Job* ch = this->childJobs.at(i);
        qDebug() << "deleting" << ch->getHint();
        delete ch;
        qDebug() << "OK";
    }

    // TODO: qDeleteAll(childJobs);
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

void Job::completeWithProgress()
{
    setProgress(1);
    complete();
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
    parentJob_ = parentJob;
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

Job* Job::newSubJob(double part, const QString &title, bool updateParentHint_,
        bool updateParentProgress_)
{
    Job* r = new Job("", this);
    r->title = title;
    if (r->title.isEmpty())
        r->title = this->getHint();
    r->subJobSteps = part;
    r->subJobStart = this->getProgress();
    r->parentHintStart = getHint();
    r->uparentHint = updateParentHint_;
    r->uparentProgress = updateParentProgress_;

    connect(this, SIGNAL(changed(const JobState&)),
            r, SLOT(parentJobChanged(const JobState&)),
            Qt::DirectConnection);

    this->childJobs.append(r);

    //qDebug() << "subJobCreated" << r->title;

    Job* top = this;
    while (top->parentJob)
        top = top->parentJob;

    top->fireSubJobCreated(r);

    return r;
}

void Job::fireSubJobCreated(Job* sub)
{
    emit subJobCreated(sub);
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
    state.title = this->title;
    state.progress = this->progress;
    state.started = this->started;
    this->mutex.unlock();

    Job* top = this;
    while (top->parentJob)
        top = top->parentJob;

    top->fireChange(state);
}

void Job::fireChange(const JobState& s)
{
    emit changed(s);
}

void Job::setProgress(double progress)
{
    this->mutex.lock();
    this->progress = progress;
    this->mutex.unlock();

    fireChange();

    if (uparentProgress)
        updateParentProgress();
    if (uparentHint)
        updateParentHint();
}

void Job::updateParentProgress()
{
    Job* parentJob_;
    double d;
    this->mutex.lock();
    parentJob_ = parentJob;
    if (parentJob_) {
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

QString Job::getTitle() const
{
    QString title_;
    this->mutex.lock();
    title_ = this->title;
    this->mutex.unlock();

    return title_;
}

void Job::setHint(const QString &hint)
{
    this->mutex.lock();
    this->hint = hint;
    // qDebug() << hint;
    this->mutex.unlock();

    fireChange();

    if (uparentHint)
        updateParentHint();
}

void Job::updateParentHint()
{
/*
    Job* parentJob_;
    QString hint_;
    QString parentHintStart_;
    this->mutex.lock();
    parentJob_ = parentJob;
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
    */
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
