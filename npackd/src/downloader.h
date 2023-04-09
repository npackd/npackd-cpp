#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>
#include <wininet.h>
#include <mutex>

#include <QTemporaryFile>
#include <QUrl>
#include <QMetaType>
#include <QObject>
#include <QWaitCondition>
#include <QCryptographicHash>

#include "job.h"

extern HWND defaultPasswordWindow;
extern std::mutex loginDialogMutex;

/**
 * Blocks execution and downloads a file over http.
 */
class Downloader: public QObject
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

    static QString inputPassword(HINTERNET hConnectHandle, DWORD dwStatus);
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
         * @brief should the file be downloaded from the Internet? The default
         * value is "true". This can be set to "false" to only get a file from
         * the WinINet cache.
         */
        bool useInternet;

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
         * @brief timeout in seconds
         */
        int timeout;

        /**
         * @brief this data will be uploaded during the request
         */
        QByteArray postData;

        /**
         * @brief header names and values
         *     Example: "Content-Type: application/x-www-form-urlencoded"
         */
        QString headers;

        /**
         * @brief user user name for the HTTP authentication or ""
         */
        QString user;

        /**
         * @brief password password for the HTTP authentication or ""
         */
        QString password;

        /**
         * @brief user user name for the HTTP proxy authentication or ""
         */
        QString proxyUser;

        /**
         * @brief password password for the HTTP proxy authentication or ""
         */
        QString proxyPassword;

        /**
         * @brief true = do not read the data
         */
        bool ignoreContent;

        /**
         * @param url http:/https:/file: URL
         */
        explicit Request(const QUrl& url): file(nullptr), interactive(true),
                parentWindow(nullptr), url(url), hashSum(false),
                alg(QCryptographicHash::Sha256), useCache(true),
                useInternet(true),
                keepConnection(true), httpMethod("GET"),
                timeout(600), ignoreContent(false) {
        }
    };

    /**
     * @brief HTTP response
     */
    class Response
    {
    public:
        /** computed hash sum, if requested */
        QString hashSum;

        /** MIME type of the response */
        QString mimeType;

        /** if not null, Content-Disposition will be stored here */
        QString contentDisposition;
    };

    /**
     * @param job job for this method
     * @param request request definition
     * @return response
     */
    static Response download(Job* job, const Request& request);

    /**
     * @brief retrieves the content-length header for an URL.
     * @param job job object
     * @param url http:, https: or file:
     * @param parentWindow window handle or 0 if not UI is required
     * @return the content-length header value or -1 if unknown
     */
    static int64_t getContentLength(Job *job, const QUrl &url,
                                    HWND parentWindow);

    /**
     * @brief HTTP download to a temporary file
     * @param job job
     * @param request HTTP request
     * @return the created temporary file or 0 if an error occured
     */
    static QTemporaryFile *downloadToTemporary(Job *job,
            const Downloader::Request &request);
private:
    /**
     * It would be nice to handle redirects explicitely so
     *    that the file name could be derived
     *    from the last URL:
     *    http://www.experts-exchange.com/Programming/System/Windows__Programming/MFC/Q_20096714.html
     * Manual authentication:
     *    http://msdn.microsoft.com/en-us/library/aa384220(v=vs.85).aspx
     *
     * @param job job object
     * @param request HTTP request
     * @param response HTTP response
     * @return "content-length" or -1 if unknown
     */
    static int64_t downloadWin(Job* job, const Downloader::Request& request,
                               Response *response);
protected:
    static QString setStringOption(HINTERNET hInternet, DWORD dwOption,
        const QString &value);
};

#endif // DOWNLOADER_H
