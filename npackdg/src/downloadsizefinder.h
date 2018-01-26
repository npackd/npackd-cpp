#ifndef DOWNLOADSIZEFINDER_H
#define DOWNLOADSIZEFINDER_H

#include <stdint.h>

#include "qmetatype.h"
#include <QObject>
#include <QQueue>
#include <QUrl>
#include <QTemporaryFile>
#include <QThread>
#include <QAtomicInt>
#include <QMutex>
#include <QTemporaryDir>
#include <QMap>
#include <QThreadPool>

#include "dbrepository.h"
#include "urlinfo.h"

/**
 * Loads files from the Internet.
 */
class DownloadSizeFinder: public QObject
{
    Q_OBJECT

    /**
     * @brief URL -> size as int64_t or -1 if unknown or -2 if an error occured
     *     The data in this field should be accessed under the mutex.
     */
    QMap<QString, URLInfo*> sizes;

    QMutex mutex;

    DBRepository* dbr;

    /**
     * @brief downloads a file
     * @param url this file should be downloaded
     * @return result
     */
    URLInfo downloadRunnable(const QString &url);
public:
    static QThreadPool threadPool;
private:
    static class _init
    {
    public:
        _init() {
            //if (threadPool.maxThreadCount() > 2)
                threadPool.setMaxThreadCount(10);
        }
    } _initializer;
public:
    /**
     * The thread is not started.
     */
    DownloadSizeFinder();

    virtual ~DownloadSizeFinder();

    /**
     * @brief download a file. This function does not block.
     * @param url this file will be downloaded
     * @return size or -2 if an error occured or -1 if the size is unknown
     */
    int64_t downloadOrQueue(const QString& url);
signals:
    /**
     * @brief a download was completed (with or without an error)
     * @param url the file from this URL was downloaded
     * @param size size of the download or -1 if unknown or -2 for an error
     */
    void downloadCompleted(const QString& url, int64_t size);
private slots:
    void watcherFinished();
};

#endif // DOWNLOADSIZEFINDER_H
