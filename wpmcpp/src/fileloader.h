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

#include "fileloaderitem.h"

/**
 * Loads files from the Internet.
 */
class FileLoader: public QThread
{
    Q_OBJECT

    /**
     * Add items to this queue and they will be downloaded.
     */
    QQueue<FileLoaderItem> work;

    int id;

    QMutex mutex;

    QTemporaryDir dir;
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
signals:
    /**
     * This signal will be fired each time something was downloaded or
     * a download ended with an error.
     */
    void downloaded(const FileLoaderItem& it);
};

#endif // FILELOADER_H
