#ifndef FILELOADER_H
#define FILELOADER_H

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

#include "fileloaderitem.h"

/**
 * Loads files from the Internet.
 */
class FileLoader: public QThread
{
    Q_OBJECT

    class DownloadFile
    {
    public:
        QString url, file, error;
    };

    /**
     * Add items to this queue and they will be downloaded.
     */
    QQueue<FileLoaderItem> work;

    /**
     * @brief URL -> local relative file name in the temporary directory or
     *     an error message if it starts with an asterisk (*). The data
     *     in this field should be accessed under the mutex.
     */
    QMap<QString, QString> files;

    QAtomicInt id;

    QMutex mutex;

    QTemporaryDir dir;

    /**
     * @brief downloads a file
     * @param url this file should be downloaded
     * @return result
     */
    DownloadFile downloadRunnable(const QString &url);
public:
    /** set this to 1 to terminate this thread. */
    QAtomicInt terminated;

    /**
     * The thread is not started.
     */
    FileLoader();

    virtual ~FileLoader() {}

    /**
     * @brief adds a work item
     * @param item work item
     */
    void addWork(const FileLoaderItem& item);

    void run();

    /**
     * @brief download a file. This function does not block.
     * @param url this file will be downloaded
     * @param err error message or ""
     * @return local file name or "" if file is being downloaded
     */
    QString downloadOrQueue(const QString& url, QString* err);
signals:
    /**
     * This signal will be fired each time something was downloaded or
     * a download ended with an error.
     */
    void downloaded(const FileLoaderItem& it);

    /**
     * @brief a download was completed (with or without an error)
     * @param url the file from this URL was downloaded
     * @param filename full file name for the downloaded file or ""
     * @param err the error message or ""
     */
    void downloadCompleted(const QString& url, const QString& filename,
            const QString& err);
private slots:
    void watcherFinished();
};

#endif // FILELOADER_H
