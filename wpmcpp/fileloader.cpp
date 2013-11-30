#include <windows.h>

#include "qdebug.h"

#include "fileloader.h"
#include "downloader.h"
#include "job.h"

FileLoader::FileLoader()
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
    while (this->terminated != 1) {
        FileLoaderItem it;
        this->mutex.lock();
        if (!this->work.isEmpty())
            it = this->work.dequeue();
        this->mutex.unlock();

        if (it.url.isEmpty())
            Sleep(1000);
        else {
            // qDebug() << "FileLoader::run " << it.url;
            Job* job = new Job();
            it.f = Downloader::download(job, it.url, 0, true);

            emit downloaded(it);
        }
    }
    CoUninitialize();
}
