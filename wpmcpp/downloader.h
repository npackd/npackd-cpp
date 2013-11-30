#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>
#include <wininet.h>
#include <stdint.h>

#include "qtemporaryfile.h"
#include "qurl.h"
#include "qmetatype.h"
#include "qobject.h"
#include "qwaitcondition.h"

#include "job.h"

/**
 * Blocks execution and downloads a file over http.
 */
class Downloader: QObject
{
    Q_OBJECT

    static void readDataFlat(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, int64_t contentLength);
    static void readDataGZip(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, int64_t contentLength);
    static void readData(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, bool gzip, int64_t contentLength);

    static bool internetReadFileFully(HINTERNET resourceHandle,
            PVOID buffer, DWORD bufferSize, PDWORD bufferLength);

    /**
     * It would be nice to handle redirects explicitely so
     *    that the file name could be derived
     *    from the last URL:
     *    http://www.experts-exchange.com/Programming/System/Windows__Programming/MFC/Q_20096714.html
     * Manual authentication:     
     *    http://msdn.microsoft.com/en-us/library/aa384220(v=vs.85).aspx
     *
     * @param file the content will be stored here
     * @param parentWindow window handle or 0 if not UI is required
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param useCache true = use Windows Internet cache on the local disk
     */
    static void downloadWin(Job* job, const QUrl& url, QFile* file,
            QString* mime, QString* contentDisposition,
            HWND parentWindow=0, QString* sha1=0, bool useCache=false);
public:
    /**
     * @param job job for this method
     * @param url this URL will be downloaded
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @return temporary file or 0 if an error occured
     * @param useCache true = use Windows Internet cache on the local disk
     */
    static QTemporaryFile* download(Job* job, const QUrl& url,
            QString* sha1=0, bool useCache=false);

    /**
     * Downloads a file.
     *
     * @param job job for this method
     * @param url this URL will be downloaded
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param file the content will be stored here
     */
    static void download(Job* job, const QUrl& url, QFile* file,
            QString* sha1=0);
};

#endif // DOWNLOADER_H
