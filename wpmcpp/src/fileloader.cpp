#include <windows.h>

#include "qdebug.h"
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

#include "fileloader.h"
#include "downloader.h"
#include "job.h"

FileLoader::FileLoader(): id(0)
{
}

QString FileLoader::downloadOrQueue(const QString &url, QString *err)
{
    QString r;
    *err = "";

    this->mutex.lock();
    if (this->files.contains(url)) {
        QString file = this->files.value(url);
        if (file.startsWith('*'))
            *err = file.mid(1);
        else
            r = dir.path() + "\\" + file;
        this->mutex.unlock();
    } else {
        this->mutex.unlock();

        QFuture<DownloadFile> future = QtConcurrent::run(this,
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

    this->mutex.lock();
    if (!r.file.isEmpty())
        this->files.insert(r.url, r.file);
    else
        this->files.insert(r.url, "*" + r.error);
    this->mutex.unlock();

    QString file = r.file;
    if (!file.isEmpty())
        file = dir.path() + "\\" + file;

    emit this->downloadCompleted(r.url, file, r.error);

    w->deleteLater();
}

FileLoader::DownloadFile FileLoader::downloadRunnable(const QString& url)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);

    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);

    CoInitialize(NULL);

    FileLoader::DownloadFile r;
    r.url = url;

    this->mutex.lock();
    if (this->files.contains(url)) {
        QString v = this->files.value(url);
        if (v.startsWith('*'))
            r.error = v.mid(1);
        else
            r.file = v;
        this->mutex.unlock();
    } else {
        this->mutex.unlock();
        QString filename = QString::number(id.fetchAndAddAcquire(1));
        QString fn = dir.path() + "\\" + filename;
        QFile f(fn);
        if (f.open(QFile::ReadWrite)) {
            QString mime;
            Job* job = new Job();
            Downloader::download(job, url, &f, 0, QCryptographicHash::Sha1,
                    true, &mime);
            f.close();

            if (!job->getErrorMessage().isEmpty()) {
                r.error = job->getErrorMessage();
            } else {
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
                f.close();
                f.rename(fn + ext);
                r.file = filename + ext;
            }
            delete job;
        } else {
            r.error = QObject::tr("Cannot open the file %1").arg(fn);
        }
    }

    CoUninitialize();

    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);

    return r;
}
