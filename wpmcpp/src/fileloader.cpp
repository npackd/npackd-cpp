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

void FileLoader::addWork(const FileLoaderItem &item)
{
    this->mutex.lock();
    if (!this->work.contains(item))
        this->work.enqueue(item);
    this->mutex.unlock();
}

void FileLoader::run()
{
    CoInitialize(NULL);
    while (this->terminated.load() != 1) {
        FileLoaderItem it;
        this->mutex.lock();
        if (!this->work.isEmpty())
            it = this->work.dequeue();
        this->mutex.unlock();

        if (it.url.isEmpty())
            Sleep(1000);
        else {
            // qDebug() << "FileLoader::run " << it.url;
            if (dir.isValid()) {
                QString fn = dir.path() + QString("\\%1.png").arg(
                        id.fetchAndAddAcquire(1));
                QFile f(fn);
                if (f.open(QFile::ReadWrite)) {
                    Job* job = new Job();
                    if (it.contentRequired)
                        Downloader::download(job, it.url, &f);
                    else
                        it.size = Downloader::getContentLength(job, it.url, 0);
                    f.close();
                    if (job->getErrorMessage().isEmpty()) {
                        it.f = fn;
                    }
                    delete job;
                }
            }

            emit downloaded(it);
        }
    }
    CoUninitialize();
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
    this->setPriority(QThread::LowestPriority);

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
        QString filename = QString("%1.png").arg(id.fetchAndAddAcquire(1));
        QString fn = dir.path() + "\\" + filename;
        QFile f(fn);
        if (f.open(QFile::ReadWrite)) {
            Job* job = new Job();
            Downloader::download(job, url, &f);
            f.close();

            if (!job->getErrorMessage().isEmpty()) {
                r.error = job->getErrorMessage();
            } else {
                r.file = filename;
            }
            delete job;
        } else {
            r.error = QObject::tr("Cannot open the file %1").arg(fn);
        }
    }

    CoUninitialize();

    return r;
}
