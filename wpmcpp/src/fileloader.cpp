#include <windows.h>

#include "qdebug.h"
#include <QFile>

#include "fileloader.h"
#include "downloader.h"
#include "job.h"

FileLoader::FileLoader(): id(0)
{
}

void FileLoader::addWork(const FileLoaderItem &item)
{
    this->mutex.lock();
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
                QString fn = dir.path() + QString("\\%1.png").arg(id++);
                QFile f(fn);
                if (f.open(QFile::ReadWrite)) {
                    Job* job = new Job();
                    Downloader::download(job, it.url, &f);
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
