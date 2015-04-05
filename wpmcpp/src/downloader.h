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

    /**
     * @brief readDataFlat
     * @param job
     * @param hResourceHandle
     * @param file 0 = ignore the read data
     * @param sha1
     * @param contentLength
     * @param alg
     */
    static void readDataFlat(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, int64_t contentLength,
            QCryptographicHash::Algorithm alg);

    static void readDataGZip(Job* job, HINTERNET hResourceHandle, QFile* file,
            QString* sha1, int64_t contentLength,
            QCryptographicHash::Algorithm alg);

    /**
     * @brief readData
     * @param job
     * @param hResourceHandle
     * @param file 0 = ignore the read data
     * @param sha1
     * @param gzip
     * @param contentLength
     * @param alg
     */
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
     * @param keepConnection true = keep the connection open
     * @param timeout download timeout in seconds
     * @return "content-length" or -1 if unknown
     */
    static int64_t downloadWin(Job* job, const QUrl& url,
            LPCWSTR verb, QFile* file,
            QString* mime, QString* contentDisposition,
            HWND parentWindow=0, QString* sha1=0, bool useCache=false,
            QCryptographicHash::Algorithm alg=QCryptographicHash::Sha1,
            bool keepConnection=true, int timeout=300);

    /**
     * Copies a file.
     *
     * This functionality also offers the possibility to have a full
     * offline support. Which means that Npackd can rely on only local files
     * without requiring any internet access. Possibilities such as USB/CD
     * installers, personal packages and collections are now a little easier
     * (some little constraints are still present).
     *
     * @param job job for this method
     * @param source source file name
     * @param file the content will be stored here. 0 means that the hash sum
     *     will be computed, but the file will be not copied
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param alg algorithm that should be used to compute the hash sum
     */
    static void copyFile(Job *job, const QString &source, QFile *file,
            QString *sha1,
            QCryptographicHash::Algorithm alg);
public:
    /**
     * @brief a download request
     */
    class Request
    {
    public:
        /**
         * the response data will be stored here. This is only a reference,
         * the object will not be freed with Request. 0 means that the response
         * will be read and discarded.
         */
        QFile* file;

        /** true = ask the user for passwords */
        bool interactive;

        /** parent window handle or 0 */
        HWND parentWindow;

        /** http:/https:/file:/data:image/png;base64 URL*/
        QUrl url;

        /** should the hash sum be computed? */
        bool hashSum;

        /** algorithm for the hash sum */
        QCryptographicHash::Algorithm alg;

        /**
         * should the cache be used? This is only applicable to http: and
         * https.
         */
        bool useCache;

        /**
         * true = keep the connection open. This is only applicable to http:
         * and https.
         */
        bool keepConnection;

        /**
         * "GET", etc. This is only applicable to http:
         * and https.
         */
        QString httpMethod;

        /**
         * if true and file==0 a new QTemporary file will be created to store
         * the downloaded file->
         */
        bool createTemporaryFile;

        /**
         * @param url http:/https:/file: URL
         */
        Request(const QUrl& url): file(0), interactive(false),
                parentWindow(0), url(url), hashSum(false),
                alg(QCryptographicHash::Sha256), useCache(true),
                keepConnection(false), httpMethod("GET"),
                createTemporaryFile(false) {
        }
    };

    /**
     * @brief HTTP response
     */
    class Response
    {
    public:
        /**
         * the response data was stored here. This is only a reference,
         * the object will not be freed with Response.
         */
        QFile* file;

        /** computed hash sum, if requested */
        QString hashSum;

        /** MIME type of the response */
        QString mimeType;
    };

    /**
     * @param job job for this method
     * @param request request definition
     * @return response
     */
    static Response download(Job* job, const Request& request);

    /**
     * @param job job for this method
     * @param url this URL will be downloaded. http://, https://, file:// and
     *     data:image/png;base64, are supported
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param alg algorithm that should be used for computing the hash sum
     * @param useCache true = use Windows Internet cache on the local disk
     * @param mime if not null, MIME type will be stored here
     * @return temporary file or 0 if an error occured. The file is closed.
     */
    static QTemporaryFile* download(Job* job, const QUrl& url,
            QString* sha1=0,
            QCryptographicHash::Algorithm alg=QCryptographicHash::Sha1,
            bool useCache=true,
            QString* mime=0);

    /**
     * QtConcurrent::run only supports 5 arguments
     *
     * @param job job for this method
     * @param url this URL will be downloaded. http://, https://, file:// and
     *     data:image/png;base64, are supported
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param alg algorithm that should be used for computing the hash sum
     * @param useCache true = use Windows Internet cache on the local disk
     * @param mime if not null, MIME type will be stored here
     * @return temporary file or 0 if an error occured. The file is closed.
     */
    static QTemporaryFile* download2(Job* job, const QUrl& url,
            bool useCache=true);

    /**
     * Downloads a file.
     *
     * @param job job for this method
     * @param url this URL will be downloaded. http://, https://, file:// and
     *     data:image/png;base64, are supported
     * @param sha1 if not null, SHA1 will be computed and stored here
     * @param file the content will be stored here. 0 means that the data will
     *     be read and discarded
     * @param alg algorithm that should be used for computing the hash sum
     * @param mime if not null, MIME type will be stored here
     * @param useCache true = use Windows Internet cache on the local disk
     * @param keepConnection true = keep the HTTP connection open
     * @param timeout download timeout for HTTP in seconds
     */
    static void download(Job* job, const QUrl& url, QFile* file,
            QString* sha1=0,
            QCryptographicHash::Algorithm alg=QCryptographicHash::Sha1,
            bool useCache=true,
            QString* mime=0,
            bool keepConnection=true, int timeout=300);

    /**
     * @brief retrieves the content-length header for an URL.
     * @param job job object
     * @param url http:, https: or file:
     * @param parentWindow window handle or 0 if not UI is required
     * @return the content-length header value or -1 if unknown
     */
    static int64_t getContentLength(Job *job, const QUrl &url,
            HWND parentWindow);
};

#endif // DOWNLOADER_H
