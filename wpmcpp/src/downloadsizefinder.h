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

/**
 * Loads files from the Internet.
 */
class DownloadSizeFinder: public QObject
{
    Q_OBJECT

    class DownloadFile
    {
    public:
        QString url, error;
        int64_t size;
    };

    /**
     * @brief URL -> size as int64_t or
     *     an error message if it starts with an asterisk (*). The data
     *     in this field should be accessed under the mutex.
     */
    QMap<QString, QString> files;

    QMutex mutex;

    /**
     * @brief downloads a file
     * @param url this file should be downloaded
     * @return result
     */
    DownloadFile downloadRunnable(const QString &url);
public:
    /**
     * The thread is not started.
     */
    DownloadSizeFinder();

    virtual ~DownloadSizeFinder() {}

    /**
     * @brief download a file. This function does not block.
     * @param url this file will be downloaded
     * @param err error message or ""
     * @return size or -1 if the file is being downloaded
     */
    int64_t downloadOrQueue(const QString& url, QString* err);
signals:
    /**
     * @brief a download was completed (with or without an error)
     * @param url the file from this URL was downloaded
     * @param size size of the download
     * @param err the error message or ""
     */
    void downloadCompleted(const QString& url, int64_t size,
            const QString& err);
private slots:
    void watcherFinished();
};

#endif // DOWNLOADSIZEFINDER_H
