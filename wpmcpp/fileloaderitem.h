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
     * the downloaded file. This object will be not freed if FileLoaderItem is
     * freed.
     */
    QTemporaryFile* f;

    FileLoaderItem();
    virtual ~FileLoaderItem();

    FileLoaderItem(const FileLoaderItem& it);

    FileLoaderItem& operator=(const FileLoaderItem& it);
};

Q_DECLARE_METATYPE(FileLoaderItem)

#endif // FILELOADERITEM_H
