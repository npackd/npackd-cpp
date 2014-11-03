#include <windows.h>

#include "qdebug.h"
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

#include "downloadsizefinder.h"
#include "downloader.h"
#include "job.h"

DownloadSizeFinder::DownloadSizeFinder()
{
}

int64_t DownloadSizeFinder::downloadOrQueue(const QString &url, QString *err)
{
    int64_t r = -1;
    *err = "";

    this->mutex.lock();
    if (this->files.contains(url)) {
        QString v = this->files.value(url);
        if (v.startsWith('*'))
            *err = v.mid(1);
        else
            r = v.toLongLong();
        this->mutex.unlock();
    } else {
        this->mutex.unlock();

        QFuture<DownloadFile> future = QtConcurrent::run(this,
                &DownloadSizeFinder::downloadRunnable, url);
        QFutureWatcher<DownloadFile>* w =
                new QFutureWatcher<DownloadFile>(this);
        connect(w, SIGNAL(finished()), this,
                SLOT(watcherFinished()));
        w->setFuture(future);
    }

    return r;
}

void DownloadSizeFinder::watcherFinished()
{
    QFutureWatcher<DownloadFile>* w = static_cast<
            QFutureWatcher<DownloadFile>*>(sender());
    DownloadFile r = w->result();

    this->mutex.lock();
    if (r.error.isEmpty())
        this->files.insert(r.url, QString::number(r.size));
    else
        this->files.insert(r.url, "*" + r.error);
    this->mutex.unlock();

    emit this->downloadCompleted(r.url, r.size, r.error);

    w->deleteLater();
}

DownloadSizeFinder::DownloadFile DownloadSizeFinder::downloadRunnable(
        const QString& url)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);

    CoInitialize(NULL);

    DownloadSizeFinder::DownloadFile r;
    r.url = url;

    this->mutex.lock();
    if (this->files.contains(url)) {
        QString v = this->files.value(url);
        if (v.startsWith('*'))
            r.error = v.mid(1);
        else
            r.size = v.toLongLong();
        this->mutex.unlock();
    } else {
        this->mutex.unlock();

        Job* job = new Job();
        r.size = Downloader::getContentLength(job, url, 0);

        if (!job->getErrorMessage().isEmpty()) {
            r.error = job->getErrorMessage();
        }

        delete job;
    }

    CoUninitialize();

    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);

    return r;
}
