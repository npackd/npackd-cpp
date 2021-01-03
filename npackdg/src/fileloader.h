#ifndef FILELOADER_H
#define FILELOADER_H

#include <chrono>
#include <random>

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
#include <QSqlError>
#include <QThreadPool>
#include <QMutex>

#include "mysqlquery.h"
#include "dbrepository.h"

/**
 * Loads files from the Internet.
 */
class FileLoader: public QObject
{
    Q_OBJECT

    class DownloadFile
    {
    public:
        /** SQLite ROWID (field ID) for the table URL */
        int64_t id;

        QString url, file, error;

        /** download size or -1 if unknown or -2 if an error occured */
        int64_t size;

        /** date/time when the size was computed */
        time_t sizeModified;
    };

    MySQLQuery* insertURLInfosQuery = nullptr;
    std::unique_ptr<MySQLQuery> updateURLQuery;

    QMutex mutex;

    /**
     * @brief downloads a file
     * @param url this file should be downloaded
     * @return result
     */
    DownloadFile downloadSizeRunnable(const QString &url);

    QString dir;

    QSqlDatabase db;

    /**
     * @brief searches for the information about an URL
     * @param url URL
     * @return (URL information, error message)
     */
    std::tuple<DownloadFile, QString> findURLInfo(const QString& url);

    /**
     * @brief downloads a file
     * @param url this file should be downloaded
     * @return result
     */
    DownloadFile downloadFileRunnable(int64_t id, const QString &url);

    QString exec(const QString &sql);
    QString open(const QString &connectionName, const QString &file);
    QString updateDatabase();

    QSet<QString> loading;
    QSet<QString> loadingSize;

    /**
     * @brief saves the download size for an URL
     * @param url URL
     * @param size size of the URL or -1 if unknown or -2 if an error occured
     * @param file file name in the cache without directory
     * @return ROWID, error message
     */
    std::tuple<int64_t, QString> saveURLInfos(
            const QString& url, int64_t size, const QString& file);
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

    QString getUnusedFilename();
public:
    /**
     * The thread is not started.
     */
    FileLoader();

    /**
     * @brief initialize the cache
     *
     * @return error message
     */
    QString init();

    virtual ~FileLoader();

    /**
     * @brief download a file. This function does not block.
     * @param url this file will be downloaded
     * @param err error message or ""
     * @return local file name or "" if file is being downloaded
     */
    QString downloadFileOrQueue(const QString& url, QString* err);

    /**
     * @brief download a file. This function does not block.
     * @param url this file will be downloaded
     * @return size or -2 if an error occured or -1 if the size is unknown
     */
    int64_t downloadSizeOrQueue(const QString& url);
signals:
    /**
     * @brief a download was completed (with or without an error)
     * @param url the file from this URL was downloaded
     * @param filename full file name for the downloaded file or ""
     * @param err the error message or ""
     */
    void downloadFileCompleted(const QString& url, const QString& filename,
            const QString& err);

    /**
     * @brief a download was completed (with or without an error)
     * @param url the file from this URL was downloaded
     * @param size size of the download or -1 if unknown or -2 for an error
     */
    void downloadSizeCompleted(const QString& url, int64_t size);
private slots:
    void watcherFileFinished();
    void watcherSizeFinished();
};

#endif // FILELOADER_H
