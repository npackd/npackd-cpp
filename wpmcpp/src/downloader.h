#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>
#include <wininet.h>
#include <stdint.h>

#include <QTemporaryFile>
#include <QUrl>
#include <QMetaType>
#include <QObject>
#include <QWaitCondition>
#include <QCryptographicHash>

#include "job.h"

/**
 * Blocks execution and downloads a file over http.
 */
class Downloader: QObject
{
    Q_OBJECT

    static void readDataFlat(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, int64_t contentLength,
            QCryptographicHash::Algorithm alg);
    static void readDataGZip(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, int64_t contentLength,
            QCryptographicHash::Algorithm alg);
    static void readData(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, bool gzip, int64_t contentLength,
            QCryptographicHash::Algorithm alg);

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
     * @param job job object
     * @param url http: or https:
     * @param verb e.g. L"GET"
     * @param file the content will be stored here, 0 = do not read the content
     * @param parentWindow window handle or 0 if not UI is required
     * @param mime if not null, MIME type will be stored here
     * @param contentDisposition if not null, Content-Disposition will be
     *     stored here
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param useCache true = use Windows Internet cache on the local disk
     * @param alg algorithm that should be used to compute the hash sum
     * @return "content-length" or -1 if unknown
     */
    static int64_t downloadWin(Job* job, const QUrl& url,
            LPCWSTR verb, QFile* file,
            QString* mime, QString* contentDisposition,
            HWND parentWindow=0, QString* sha1=0, bool useCache=false,
            QCryptographicHash::Algorithm alg=QCryptographicHash::Sha1);
public:
    /**
     * @param job job for this method
     * @param url this URL will be downloaded. http://, https:// and
     *     data:image/png;base64, are supported
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param alg algorithm that should be used for computing the hash sum
     * @param useCache true = use Windows Internet cache on the local disk
     * @return temporary file or 0 if an error occured. The file is closed.
     */
    static QTemporaryFile* download(Job* job, const QUrl& url,
            QString* sha1=0,
            QCryptographicHash::Algorithm alg=QCryptographicHash::Sha1,
            bool useCache=true);

    /**
     * Downloads a file.
     *
     * @param job job for this method
     * @param url this URL will be downloaded. http://, https:// and
     *     data:image/png;base64, are supported
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param file the content will be stored here
     * @param alg algorithm that should be used for computing the hash sum
     * @param useCache true = use Windows Internet cache on the local disk
     */
    static void download(Job* job, const QUrl& url, QFile* file,
            QString* sha1=0,
            QCryptographicHash::Algorithm alg=QCryptographicHash::Sha1,
            bool useCache=true);

    /**
     * @brief retrieves the content-length header for an URL
     * @param job job object
     * @param url http: or https:
     * @param parentWindow window handle or 0 if not UI is required
     * @return the content-length header value or -1 if unknown
     */
    static int64_t getContentLength(Job *job, const QUrl &url,
            HWND parentWindow);
};

#endif // DOWNLOADER_H
