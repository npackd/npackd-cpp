#include <cmath>
#include <time.h>

#include <QLoggingCategory>

#include "wpmutils.h"

#include "job.h"

Job::Job(const QString &title, Job *parent): parentJob(parent)
{
    this->title = title;
    this->progress = 0.0;
    this->subJobStart = 0;
    this->subJobSteps = -1;
    this->cancelRequested = false;
    this->completed = false;
    this->started = 0;
    this->uparentProgress = true;
    this->updateParentErrorMessage = false;
}

Job::~Job()
{
    qDeleteAll(childJobs);
}

time_t Job::remainingTime()
{
    this->mutex.lock();
    time_t started_ = this->started;
    double progress_ = this->progress;
    this->mutex.unlock();

    time_t result;
    if (started != 0) {
        time_t now;
        time(&now);

        time_t diff = static_cast<time_t>(difftime(now, started_));
        diff = lround(diff * (1 / progress_ - 1));
        result = diff;
    } else {
        result = 0;
    }
    return result;
}

time_t Job::getStarted()
{
    time_t started_;
    this->mutex.lock();
    started_ = this->started;
    this->mutex.unlock();

    return started_;
}

void Job::waitFor()
{
    waitForChildren();
    while (!isCompleted()) {
        Sleep(100);
    }
}

void Job::waitForChildren()
{
    this->mutex.lock();
    for (auto ch: this->childJobs) {
        if (!ch->isCompleted()) {
            ch->waitFor();
        }
    }
    this->mutex.unlock();
}

void Job::complete()
{
    // waitForChildren();

    bool completed_;
    this->mutex.lock();
    bool f = false;
    completed_ = this->completed;
    if (!completed_) {
        this->completed = true;
        f = true;
    }
    this->mutex.unlock();

    if (f)
        emit jobCompleted();
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
    this->mutex.lock();
    bool changed = false;
    if (!this->cancelRequested && this->errorMessage.isEmpty()) {
        this->cancelRequested = true;
        changed = true;
    }
    this->mutex.unlock();

    if (changed) {
        fireChange();

        this->mutex.lock();
        for (auto ch: this->childJobs) {
            ch->cancel();
        }
        this->mutex.unlock();
    }
}

void Job::parentJobChanged(Job* s)
{
    if (s->isCancelled() && !this->isCancelled())
        this->cancel();
}

Job* Job::newSubJob(double part, const QString &title,
        bool updateParentProgress_,
        bool updateParentErrorMessage)
{
    Job* r = new Job("", this);
    r->title = title;
    r->subJobSteps = part;
    r->subJobStart = this->getProgress();
    r->uparentProgress = updateParentProgress_;
    r->updateParentErrorMessage = updateParentErrorMessage;

    connect(this, SIGNAL(changed(Job*)),
            r, SLOT(parentJobChanged(Job*)),
            Qt::DirectConnection);

    this->childJobs.push_back(r);

    //qCDebug(npackd) << "subJobCreated" << r->title;

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

    bool t = false;
    if (this->timeout > 0) {
        time_t now;
        time(&now);
        double d = difftime(now, this->started);
        if (d > this->timeout) {
            t = true;
        }
    }
    this->mutex.unlock();

    Job* top = this;
    while (top->parentJob)
        top = top->parentJob;

    top->fireChange(this);

    if (t)
        this->cancel();
}

void Job::fireChange(Job* s)
{
    emit changed(s);
}

void Job::setProgress(double progress)
{
    bool changed = false;

    this->mutex.lock();
    if (progress > 1.0001) {
        qCDebug(npackd) << "Job: progress =" << progress << "in" << this->title;
    }
    if (progress < this->progress) {
        qCDebug(npackd) << "Job: stepping back from" << this->progress <<
                "to" << progress << "in" << getFullTitle();
    }
    if (fabs(progress - this->progress) > 0.005) {
        this->progress = progress;
        changed = true;
    }
    this->mutex.unlock();

    if (changed) {
        fireChange();

        if (uparentProgress)
            updateParentProgress();
    }
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

int Job::getLevel() const
{
    int r = 0;

    this->mutex.lock();
    if (parentJob)
        r = parentJob->getLevel() + 1;
    this->mutex.unlock();

    return r;
}

const Job *Job::getTopJob() const
{
    const Job* r = nullptr;

    this->mutex.lock();
    r = this;
    while (r->parentJob)
        r = r->parentJob;
    this->mutex.unlock();

    return r;
}

QString Job::getTitle() const
{
    QString title_;
    this->mutex.lock();
    title_ = this->title;
    this->mutex.unlock();

    return title_;
}

QString Job::getFullTitle() const
{
    QString title_;
    this->mutex.lock();
    title_ = this->title;
    Job* v = this->parentJob;
    while (v) {
        title_ = v->getTitle() + " / " + title_;
        v = v->parentJob;
    }

    this->mutex.unlock();

    return title_;
}

void Job::setTimeout(int timeout)
{
    this->mutex.lock();
    this->timeout = timeout;
    this->mutex.unlock();

    fireChange();
}

void Job::setTitle(const QString &title)
{
    this->mutex.lock();
    this->title = title;
    // qCDebug(npackd) << hint;
    this->mutex.unlock();

    fireChange();
}

void Job::checkOSCall(bool v)
{
    if (!v) {
        QString msg;
        WPMUtils::formatMessage(GetLastError(), &msg);
        setErrorMessage(msg);
    }
}

void Job::checkHResult(HRESULT v)
{
    if (FAILED(v)) {
        QString err;
        WPMUtils::formatMessage(v, &err);
        setErrorMessage(err);
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

void Job::checkTimeout()
{
    this->mutex.lock();
    bool t = false;
    if (this->timeout > 0) {
        time_t now;
        time(&now);
        double d = difftime(now, this->started);
        if (d > this->timeout) {
            t = true;
        }
    }

    if (parentJob)
        parentJob->checkTimeout();
    this->mutex.unlock();

    if (t)
        cancel();
}

bool Job::shouldProceed() const
{
    return !this->isCancelled() && this->getErrorMessage().isEmpty();
}

void Job::setErrorMessage(const QString &errorMessage)
{
    this->mutex.lock();
    bool changed = false;
    if (!errorMessage.isEmpty() && this->errorMessage != errorMessage) {
        this->errorMessage = errorMessage;
        if (parentJob && updateParentErrorMessage) {
            QString msg = title + ": " + errorMessage;

            parentJob->setErrorMessage(msg);
        }
        changed = true;
    }
    this->mutex.unlock();

    if (changed) {
        fireChange();

        this->mutex.lock();
        for (auto ch: childJobs) {
            ch->cancel();
        }
        this->mutex.unlock();
    }
}

