#ifndef FILELOADERITEM_H
#define FILELOADERITEM_H

#include "qobject.h"
#include "qtemporaryfile.h"
#include "qmetatype.h"

/**
 * A work item for FileLoader.
 */
class FileLoaderItem: public QObject {
public:
    /** this file will be downloaded */
    QString url;

    /**
     * the downloaded file or "" if an error occured. The file will be
     * automatically removed during the program shutdown.
     */
    QString f;

    FileLoaderItem();
    virtual ~FileLoaderItem();

    FileLoaderItem(const FileLoaderItem& it);

    FileLoaderItem& operator=(const FileLoaderItem& it);

    /**
     * @param it another object
     * @return true if the "url" is the same
     */
    bool operator==(const FileLoaderItem& it);
};

Q_DECLARE_METATYPE(FileLoaderItem)

#endif // FILELOADERITEM_H
