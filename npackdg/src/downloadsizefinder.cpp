#include <windows.h>
#include <ctime>

#include <QtGlobal>
#if defined(_MSC_VER) && (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

#include "downloadsizefinder.h"
#include "downloader.h"
#include "job.h"
#include "concurrent.h"
#include "wpmutils.h"

extern HWND defaultPasswordWindow;

QThreadPool DownloadSizeFinder::threadPool;
DownloadSizeFinder::_init DownloadSizeFinder::_initializer;

int64_t DownloadSizeFinder::downloadOrQueue(const QString &url)
{
    int64_t r;

    this->mutex.lock();
    URLInfo* v = this->sizes.value(url);
    if (v) {
        r = v->size;

        // if the value is older than 10 days, it is considered obsolete
        if (r >= 0 && time(nullptr) - v->sizeModified > 10 * 24 * 60 * 60) {
            r = -1;
        }
    } else {
        r = -1;
    }
    this->mutex.unlock();

    if (r == -1) {
        this->mutex.lock();
        QFuture<URLInfo> future = run(&threadPool, this,
                &DownloadSizeFinder::downloadRunnable, url);
        QFutureWatcher<URLInfo>* w =
                new QFutureWatcher<URLInfo>(this);
        connect(w, SIGNAL(finished()), this, SLOT(watcherFinished()));
        w->setFuture(future);
        this->mutex.unlock();
    }

    return r;
}

void DownloadSizeFinder::watcherFinished()
{
    QFutureWatcher<URLInfo>* w = static_cast<
            QFutureWatcher<URLInfo>*>(sender());

    URLInfo r = w->result();

    this->mutex.lock();
    dbr->saveURLSize(r.address, r.size);
    this->mutex.unlock();

    this->mutex.lock();
    URLInfo* v = this->sizes.value(r.address);
    if (!v) {
        v = new URLInfo(r.address);
        this->sizes.insert(r.address, v);
    }
    v->size = r.size;
    v->sizeModified = r.sizeModified;
    this->mutex.unlock();

    emit this->downloadCompleted(r.address, r.size);

    w->deleteLater();
}

URLInfo DownloadSizeFinder::downloadRunnable(
        const QString& url)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);

    /*
    makes the process too slow
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);
    */

    CoInitialize(nullptr);

    URLInfo r(url);

    this->mutex.lock();
    if (!this->dbr) {
        dbr = new DBRepository();
        QString err = dbr->openDefault("defaultDownloadSizeFinder");
        qCDebug(npackd) << "DownloadSizeFinder::downloadRunnable.openDefault" << err;
        if (err.isEmpty()) {
            sizes = dbr->findURLInfos(&err);
            qCDebug(npackd) << "DownloadSizeFinder::downloadRunnable.sizes" << err;
        }
    }
    this->mutex.unlock();

    this->mutex.lock();
    URLInfo* v = this->sizes.value(url);
    if (v) {
        r = *v;
    }
    this->mutex.unlock();

    // if the value is older than 10 days, it is considered obsolete
    if (r.size >= 0 && time(nullptr) - r.sizeModified > 10 * 24 * 60 * 60) {
        r.size = -1;
    }

    if (r.size < 0) {
        Job* job = new Job();
        r.size = Downloader::getContentLength(job, url, defaultPasswordWindow);
        r.sizeModified = time(nullptr);

        if (!job->getErrorMessage().isEmpty() || r.size < 0) {
            r.size = -2;
        }

        delete job;
    }

    CoUninitialize();

    /*
    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    */

    return r;
}

DownloadSizeFinder::DownloadSizeFinder(): dbr(nullptr)
{
}

DownloadSizeFinder::~DownloadSizeFinder()
{
    qDeleteAll(this->sizes);
    this->sizes.clear();
    delete this->dbr;
}
