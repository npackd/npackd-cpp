#include <windows.h>

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

#include "fileloader.h"
#include "downloader.h"
#include "job.h"
#include "downloadsizefinder.h"
#include "concurrent.h"

FileLoader::FileLoader(): id(0)
{
}

QString FileLoader::downloadOrQueue(const QString &url, QString *err)
{
    QString r;
    *err = "";

    this->mutex.lock();
    if (this->files.contains(url)) {
        DownloadFile file = this->files.value(url);
        if (!file.error.isEmpty())
            *err = file.error;
        else if (!file.file.isEmpty())
            r = dir.path() + "\\" + file.file;
        this->mutex.unlock();
    } else {
        this->mutex.unlock();

        QFuture<DownloadFile> future = run(
                &DownloadSizeFinder::threadPool, this,
                &FileLoader::downloadRunnable, url);
        QFutureWatcher<DownloadFile>* w =
                new QFutureWatcher<DownloadFile>(this);
        connect(w, SIGNAL(finished()), this,
                SLOT(watcherFinished()));
        w->setFuture(future);
    }

    return r;
}

void FileLoader::watcherFinished()
{
    QFutureWatcher<DownloadFile>* w = static_cast<
            QFutureWatcher<DownloadFile>*>(sender());
    DownloadFile r = w->result();

    if (!r.file.isEmpty() || !r.error.isEmpty()) {
        QString file = r.file;
        if (!file.isEmpty())
            file = dir.path() + "\\" + file;

        emit this->downloadCompleted(r.url, file, r.error);
    }

    w->deleteLater();
}

FileLoader::DownloadFile FileLoader::downloadRunnable(const QString& url)
{
    FileLoader::DownloadFile r;
    r.url = url;

    this->mutex.lock();
    if (this->files.contains(url)) {
        r = this->files.value(url);
        this->mutex.unlock();
    } else {
        this->files.insert(url, r);
        this->mutex.unlock();

        QThread::currentThread()->setPriority(QThread::LowestPriority);

        bool b = SetThreadPriority(GetCurrentThread(),
                THREAD_MODE_BACKGROUND_BEGIN);

        CoInitialize(nullptr);

        QString filename = QString::number(id.fetchAndAddAcquire(1));
        QString fn = dir.path() + "\\" + filename;
        QFile f(fn);
        if (f.open(QFile::ReadWrite)) {
            Job* job = new Job();
            Downloader::Request request{QUrl(url)};
            request.file = &f;
            request.useCache = true;
            request.keepConnection = false;
            request.timeout = 15;

            Downloader::Response response = Downloader::download(job, request);
            QString mime = response.mimeType;
            f.close();

            if (!job->getErrorMessage().isEmpty()) {
                r.error = job->getErrorMessage();
            } else {
                // supported extensions:
                // "bmp", "cur", "dds", "gif", "icns", "ico", "jp2", "jpeg",
                // "jpg", "mng", "pbm", "pgm", "png", "ppm", "tga", "tif",
                // "tiff", "wbmp", "webp", "xbm", "xpm"
                QString ext;
                if (mime == "image/png")
                    ext = ".png";
                else if (mime == "image/x-icon" || mime == "image/vnd.microsoft.icon")
                    ext = ".ico";
                else if (mime == "image/jpeg")
                    ext = ".jpg";
                else if (mime == "image/gif")
                    ext = ".gif";
                else if (mime == "image/x-windows-bmp" || mime == "image/bmp")
                    ext = ".bmp";
                else
                    ext = ".png";

                // qCDebug(npackd) << ext;
                f.close();
                f.rename(fn + ext);
                r.file = filename + ext;
            }
            delete job;
        } else {
            r.error = QObject::tr("Cannot open the file %1").arg(fn);
        }

        this->mutex.lock();
        this->files.insert(r.url, r);
        this->mutex.unlock();

        CoUninitialize();

        if (b)
            SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    }

    return r;
}
