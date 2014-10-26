#include <math.h>
#include <stdint.h>

#include <windows.h>
#include <wininet.h>

#include <zlib.h>

#include <QObject>
#include <QDebug>
#include <QWaitCondition>
#include <QMutex>
#include <QCryptographicHash>

#include "downloader.h"
#include "job.h"
#include "wpmutils.h"

HWND defaultPasswordWindow = 0;

int64_t Downloader::downloadWin(Job* job, const QUrl& url, LPCWSTR verb,
        QFile* file,
        QString* mime, QString* contentDisposition,
        HWND parentWindow, QString* sha1, bool useCache,
        QCryptographicHash::Algorithm alg)
{
    QString initialTitle = job->getTitle();

    job->setTitle(initialTitle + " / " + QObject::tr("Connecting"));

    if (sha1)
        sha1->clear();

    QString server = url.host();
    QString resource = url.path();
    QString encQuery = url.query(QUrl::FullyEncoded);
    if (!encQuery.isEmpty())
        resource.append('?').append(encQuery);

    QString agent("Npackd/");
    agent.append(NPACKD_VERSION);

    agent += " (compatible; MSIE 9.0)";

    void* internet = InternetOpenW((WCHAR*) agent.utf16(),
            INTERNET_OPEN_TYPE_PRECONFIG,
            0, 0, 0);

    if (internet == 0) {
        QString errMsg;
        WPMUtils::formatMessage(GetLastError(), &errMsg);
        job->setErrorMessage(errMsg);
        job->complete();
        return -1L;
    }

    job->setProgress(0.01);

    INTERNET_PORT port = url.port(url.scheme() == "https" ?
            INTERNET_DEFAULT_HTTPS_PORT: INTERNET_DEFAULT_HTTP_PORT);
    HINTERNET hConnectHandle = InternetConnectW(internet,
            (WCHAR*) server.utf16(), port, 0, 0, INTERNET_SERVICE_HTTP, 0, 0);

    if (hConnectHandle == 0) {
        QString errMsg;
        WPMUtils::formatMessage(GetLastError(), &errMsg);
        job->setErrorMessage(errMsg);
        job->complete();
        return -1L;
    }

    if (job->isCancelled()) {
        InternetCloseHandle(internet);
        job->complete();
        return -1L;
    }

    // qDebug() << "download.4";

    // flags: http://msdn.microsoft.com/en-us/library/aa383661(v=vs.85).aspx
    // We support accepting any mime file type since this is a simple download
    // of a file
    LPCTSTR ppszAcceptTypes[2];
    ppszAcceptTypes[0] = L"*/*";
    ppszAcceptTypes[1] = NULL;
    DWORD flags = (url.scheme() == "https" ? INTERNET_FLAG_SECURE : 0) |
            INTERNET_FLAG_KEEP_CONNECTION;
    flags |= INTERNET_FLAG_RESYNCHRONIZE;
    if (!useCache)
        flags |= INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_PRAGMA_NOCACHE |
                INTERNET_FLAG_RELOAD;
    HINTERNET hResourceHandle = HttpOpenRequestW(hConnectHandle, verb,
            (WCHAR*) resource.utf16(),
            0, 0, ppszAcceptTypes,
            flags, 0);
    if (hResourceHandle == 0) {
        QString errMsg;
        WPMUtils::formatMessage(GetLastError(), &errMsg);
        job->setErrorMessage(errMsg);
        job->complete();
        return -1L;
    }

    if (!HttpAddRequestHeadersW(hResourceHandle,
            L"Accept-Encoding: gzip, deflate", -1,
            HTTP_ADDREQ_FLAG_ADD)) {
        QString errMsg;
        WPMUtils::formatMessage(GetLastError(), &errMsg);
        job->setErrorMessage(errMsg);
        job->complete();
        return -1L;
    }

    // qDebug() << "download.5";
    while (true) {
        // qDebug() << "download.5.1";

        if (!HttpSendRequestW(hResourceHandle, 0, 0, 0, 0)) {
            DWORD e = GetLastError();
            if (e) {
                // qDebug() << "error in HttpSendRequestW";
                QString errMsg;
                WPMUtils::formatMessage(e, &errMsg);
                job->setErrorMessage(errMsg);
                break;
            }
        }

        if (parentWindow) {
            void* p;
            DWORD flags = FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
                          FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS |
                          FLAGS_ERROR_UI_FLAGS_GENERATE_DATA;
            DWORD r = InternetErrorDlg(parentWindow,
                    hResourceHandle, ERROR_SUCCESS, flags, &p);
            if (r == ERROR_SUCCESS)
                break;
            else if (r == ERROR_INTERNET_FORCE_RETRY)
                ; // nothing
            else if (r == ERROR_CANCELLED) {
                job->setErrorMessage(QObject::tr("Cancelled by the user"));
                break;
            } else if (r == ERROR_INVALID_HANDLE) {
                job->setErrorMessage(QObject::tr("Invalid handle"));
                break;
            } else {
                job->setErrorMessage(QString(
                        QObject::tr("Unknown error %1 from InternetErrorDlg")).arg(r));
                break;
            }
        } else {
            // http://msdn.microsoft.com/en-us/library/aa384220(v=vs.85).aspx
            DWORD dwStatus, dwStatusSize = sizeof(dwStatus);
            if (!HttpQueryInfo(hResourceHandle, HTTP_QUERY_FLAG_NUMBER |
                    HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, NULL)) {
                QString errMsg;
                WPMUtils::formatMessage(GetLastError(), &errMsg);
                job->setErrorMessage(errMsg);
                break;
            }

            QString username, password;
            if (dwStatus == HTTP_STATUS_PROXY_AUTH_REQ) {
                WPMUtils::outputTextConsole("\n" + QObject::tr("The HTTP proxy requires authentication.") + "\n");
                WPMUtils::outputTextConsole(QObject::tr("Username") + ": ");
                username = WPMUtils::inputTextConsole();
                WPMUtils::outputTextConsole(QObject::tr("Password") + ": ");
                password = WPMUtils::inputPasswordConsole();

                if (!InternetSetOptionW(hConnectHandle,
                        INTERNET_OPTION_PROXY_USERNAME,
                        (void*) username.utf16(),
                        username.length() + 1)) {
                    QString errMsg;
                    WPMUtils::formatMessage(GetLastError(), &errMsg);
                    job->setErrorMessage(errMsg);
                    goto out;
                }
                if (!InternetSetOptionW(hConnectHandle,
                        INTERNET_OPTION_PROXY_PASSWORD,
                        (void*) password.utf16(),
                        password.length() + 1)) {
                    QString errMsg;
                    WPMUtils::formatMessage(GetLastError(), &errMsg);
                    job->setErrorMessage(errMsg);
                    goto out;
                }
            } else if (dwStatus == HTTP_STATUS_DENIED) {
                WPMUtils::outputTextConsole("\n" +
                        QObject::tr("The HTTP server requires authentication.") +
                        "\n");
                WPMUtils::outputTextConsole(QObject::tr("Username") + ": ");
                username = WPMUtils::inputTextConsole();
                WPMUtils::outputTextConsole(QObject::tr("Password") + ": ");
                password = WPMUtils::inputPasswordConsole();

                if (!InternetSetOptionW(hConnectHandle,
                        INTERNET_OPTION_USERNAME,
                        (void*) username.utf16(),
                        username.length() + 1)) {
                    QString errMsg;
                    WPMUtils::formatMessage(GetLastError(), &errMsg);
                    job->setErrorMessage(errMsg);
                    goto out;
                }
                if (!InternetSetOptionW(hConnectHandle,
                        INTERNET_OPTION_PASSWORD,
                        (void*) password.utf16(),
                        password.length() + 1)) {
                    QString errMsg;
                    WPMUtils::formatMessage(GetLastError(), &errMsg);
                    job->setErrorMessage(errMsg);
                    goto out;
                }
            } else if (dwStatus == HTTP_STATUS_OK) {
                break;
            } else {
                job->setErrorMessage(QString(
                        QObject::tr("Cannot handle HTTP status code %1")).
                        arg(dwStatus));
                break;
            }

            // read all the data before re-sending the request
            char smallBuffer[4 * 1024];
            while (true) {
                DWORD read;
                if (!InternetReadFile(hResourceHandle, &smallBuffer,
                        sizeof(smallBuffer), &read)) {
                    QString errMsg;
                    WPMUtils::formatMessage(GetLastError(), &errMsg);
                    job->setErrorMessage(errMsg);
                    goto out;
                }

                // qDebug() << "read some bytes " << read;
                if (read == 0)
                    break;
            }
        }
    };

out:
    job->setProgress(0.03);
    if (!job->getErrorMessage().isEmpty()) {
        job->complete();
        return -1L;
    }

    job->setTitle(initialTitle + " / " + QObject::tr("Downloading"));

    // MIME type
    if (mime) {
        WCHAR mimeBuffer[1024];
        DWORD bufferLength = sizeof(mimeBuffer);
        DWORD index = 0;
        if (!HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CONTENT_TYPE,
                &mimeBuffer, &bufferLength, &index)) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
            job->complete();
            return -1L;
        }
        mime->setUtf16((ushort*) mimeBuffer, bufferLength / 2);
    }

    // qDebug() << "querying Content-Encoding type";
    WCHAR contentEncodingBuffer[1024];
    DWORD bufferLength = sizeof(contentEncodingBuffer);
    DWORD index = 0;
    bool gzip = false;
    if (HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CONTENT_ENCODING,
            &contentEncodingBuffer, &bufferLength, &index)) {
        QString contentEncoding;
        contentEncoding.setUtf16((ushort*) contentEncodingBuffer,
                bufferLength / 2);
        gzip = contentEncoding == "gzip" || contentEncoding == "deflate";
    }

    job->setProgress(0.04);

    // Content-Disposition
    if (contentDisposition) {
        WCHAR cdBuffer[1024];
        wcscpy(cdBuffer, L"Content-Disposition");
        bufferLength = sizeof(cdBuffer);
        index = 0;
        if (HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CUSTOM,
                &cdBuffer, &bufferLength, &index)) {
            contentDisposition->setUtf16((ushort*) cdBuffer, bufferLength / 2);
        }
    }

    // content length
    WCHAR contentLengthBuffer[100];
    bufferLength = sizeof(contentLengthBuffer);
    index = 0;
    int64_t contentLength = -1;
    if (HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CONTENT_LENGTH,
            contentLengthBuffer, &bufferLength, &index)) {
        QString s;
        s.setUtf16((ushort*) contentLengthBuffer, bufferLength / 2);
        bool ok;
        contentLength = s.toLongLong(&ok, 10);
        if (!ok)
            contentLength = 0;
    }

    job->setProgress(0.05);

    Job* sub = job->newSubJob(0.95, QObject::tr("Reading the data"));
    if (file)
        readData(sub, hResourceHandle, file, sha1, gzip, contentLength, alg);
    if (!sub->getErrorMessage().isEmpty())
        job->setErrorMessage(sub->getErrorMessage());

    InternetCloseHandle(internet);

    job->setProgress(1);

    job->complete();

    return contentLength;
}

bool Downloader::internetReadFileFully(HINTERNET resourceHandle,
        PVOID buffer, DWORD bufferSize, PDWORD bufferLength)
{
    DWORD alreadyRead = 0;
    bool result;
    while (true) {
        DWORD len;
        result = InternetReadFile(resourceHandle,
                ((char*) buffer) + alreadyRead,
                bufferSize - alreadyRead, &len);

        if (!result) {
            *bufferLength = alreadyRead;
            break;
        } else {
            alreadyRead += len;

            if (alreadyRead == bufferSize || len == 0) {
                *bufferLength = alreadyRead;
                break;
            }
        }
    }
    return result;
}

void Downloader::readDataGZip(Job* job, HINTERNET hResourceHandle, QFile* file,
        QString* sha1, int64_t contentLength, QCryptographicHash::Algorithm alg)
{
    QString initialTitle = job->getTitle();

    // download/compute SHA1 loop
    QCryptographicHash hash(alg);
    const int bufferSize = 512 * 1024;
    unsigned char* buffer = new unsigned char[bufferSize];
    const int buffer2Size = 512 * 1024;
    unsigned char* buffer2 = new unsigned char[buffer2Size];

    bool zlibStreamInitialized = false;
    z_stream d_stream;

    int err = 0;
    int64_t alreadyRead = 0;
    DWORD bufferLength;
    do {
        if (!internetReadFileFully(hResourceHandle, buffer,
                bufferSize, &bufferLength)) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
            break;
        }

        if (bufferLength == 0)
            break;

        // http://www.gzip.org/zlib/rfc-gzip.html
        if (!zlibStreamInitialized) {
            unsigned int cur = 0;

            /*
            if (cur + 10 > bufferLength) {
                job->setErrorMessage("Less than 10 bytes");
                goto out;
            }

            unsigned char flg = buffer[3];
            cur = 10;

            // FLG.FEXTRA
            if (flg & 4) {
                uint16_t xlen;
                if (cur + 2 > bufferLength) {
                    job->setErrorMessage("XLEN missing");
                    goto out;
                } else {
                    xlen = *((uint16_t*) (buffer + cur));
                    cur += 2;
                }

                if (cur + xlen > bufferLength) {
                    job->setErrorMessage("EXTRA missing");
                    goto out;
                } else {
                    cur += xlen;
                }
            }

            // FLG.FNAME
            if (flg & 8) {
                while (true) {
                    if (cur + 1 > bufferLength) {
                        job->setErrorMessage("FNAME missing");
                        goto out;
                    } else {
                        uint8_t c = *((uint8_t*) (buffer + cur));
                        cur++;
                        if (c == 0)
                            break;
                    }
                }
            }

            // FLG.FCOMMENT
            if (flg & 16) {
                while (true) {
                    if (cur + 1 > bufferLength) {
                        job->setErrorMessage("COMMENT missing");
                        goto out;
                    } else {
                        uint8_t c = *((uint8_t*) (buffer + cur));
                        cur++;
                        if (c == 0)
                            break;
                    }
                }
            }

            // FLG.FHCRC
            if (flg & 2) {
                if (cur + 2 > bufferLength) {
                    job->setErrorMessage("CRC16 missing");
                    goto out;
                } else {
                    cur += 2;
                }
            }*/

            d_stream.zalloc = (alloc_func) 0;
            d_stream.zfree = (free_func) 0;
            d_stream.opaque = (voidpf) 0;

            d_stream.next_in = buffer + cur;
            d_stream.avail_in = bufferLength - cur;
            d_stream.avail_out = buffer2Size;
            d_stream.next_out = buffer2;
            zlibStreamInitialized = true;

            // 15 = maximum buffer size, 32 = zlib and gzip formats are parsed
            int err = inflateInit2(&d_stream, 15 + 32);
            if (err != Z_OK) {
                job->setErrorMessage(QString(QObject::tr("zlib error %1")).
                        arg(err));
                break;
            }
        } else {
            d_stream.next_in = buffer;
            d_stream.avail_in = bufferLength;
        }

        // see http://zlib.net/zpipe.c
        do {
            d_stream.avail_out = buffer2Size;
            d_stream.next_out = buffer2;

            int err = inflate(&d_stream, Z_NO_FLUSH);
            if (err == Z_NEED_DICT) {
                job->setErrorMessage(QString(QObject::tr("zlib error %1")).
                        arg(err));
                err = Z_DATA_ERROR;
                inflateEnd(&d_stream);
                break;
            } else if (err == Z_MEM_ERROR || err == Z_DATA_ERROR) {
                job->setErrorMessage(QString(QObject::tr("zlib error %1")).
                        arg(err));
                inflateEnd(&d_stream);
                break;
            } else {
                if (sha1)
                    hash.addData((char*) buffer2,
                            buffer2Size - d_stream.avail_out);

                file->write((char*) buffer2,
                        buffer2Size - d_stream.avail_out);
            }
        } while (d_stream.avail_out == 0);

        if (!job->getErrorMessage().isEmpty())
            break;

        alreadyRead += bufferLength;
        if (contentLength > 0) {
            job->setProgress(((double) alreadyRead) / contentLength);
            job->setTitle(initialTitle + " / " +
                    QString(QObject::tr("%L0 of %L1 bytes")).
                    arg(alreadyRead).
                    arg(contentLength));
        } else {
            job->setProgress(0.5);
            job->setTitle(initialTitle + " / " +
                    QString(QObject::tr("%L0 bytes")).
                    arg(alreadyRead));
        }
    } while (bufferLength != 0 && !job->isCancelled());

    err = inflateEnd(&d_stream);
    if (err != Z_OK) {
        job->setErrorMessage(QString(QObject::tr("zlib error %1")).
                arg(err));
    }

    if (sha1 && !job->isCancelled() && job->getErrorMessage().isEmpty())
        *sha1 = hash.result().toHex().toLower();

// out:
    delete[] buffer;
    delete[] buffer2;

    if (!job->isCancelled() && job->getErrorMessage().isEmpty())
        job->setProgress(1);

    job->complete();
}

void Downloader::readDataFlat(Job* job, HINTERNET hResourceHandle, QFile* file,
        QString* sha1, int64_t contentLength, QCryptographicHash::Algorithm alg)
{
    QString initialTitle = job->getTitle();

    // download/compute SHA1 loop
    QCryptographicHash hash(alg);
    const int bufferSize = 512 * 1024;
    unsigned char* buffer = new unsigned char[bufferSize];

    int64_t alreadyRead = 0;
    DWORD bufferLength;
    do {
        // gzip-header is at least 10 bytes long
        if (!InternetReadFile(hResourceHandle, buffer,
                bufferSize, &bufferLength)) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
            break;
        }

        if (bufferLength == 0)
            break;

        // update SHA1 if necessary
        if (sha1)
            hash.addData((char*) buffer, bufferLength);

        file->write((char*) buffer, bufferLength);

        alreadyRead += bufferLength;
        if (contentLength > 0) {
            job->setProgress(((double) alreadyRead) / contentLength);
            job->setTitle(initialTitle + " / " +
                    QString(QObject::tr("%L0 of %L1 bytes")).
                    arg(alreadyRead).
                    arg(contentLength));
        } else {
            job->setProgress(0.5);
            job->setTitle(initialTitle + " / " +
                    QString(QObject::tr("%L0 bytes")).
                    arg(alreadyRead));
        }
    } while (bufferLength != 0 && !job->isCancelled());

    if (!job->isCancelled() && job->getErrorMessage().isEmpty())
        job->setProgress(1);

    if (sha1 && !job->isCancelled() && job->getErrorMessage().isEmpty())
        *sha1 = hash.result().toHex().toLower();

    delete[] buffer;

    job->complete();
}

void Downloader::readData(Job* job, HINTERNET hResourceHandle, QFile* file,
        QString* sha1, bool gzip, int64_t contentLength,
        QCryptographicHash::Algorithm alg)
{
    if (gzip)
        readDataGZip(job, hResourceHandle, file, sha1, contentLength, alg);
    else
        readDataFlat(job, hResourceHandle, file, sha1, contentLength, alg);
}

void Downloader::download(Job* job, const QUrl& url, QFile* file,
        QString* sha1, QCryptographicHash::Algorithm alg, bool useCache)
{
    QString mime;
    QString contentDisposition;

    if (url.scheme() == "https" || url.scheme() == "http")
        downloadWin(job, url, L"GET", file, &mime, &contentDisposition,
                defaultPasswordWindow, sha1, useCache, alg);
    else if (url.toString().startsWith("data:image/png;base64,")) {
        QString dataURL_ = url.toString().mid(22);
        QByteArray ba = QByteArray::fromBase64(dataURL_.toLatin1());
        if (file->write(ba) < 0)
            job->setErrorMessage(file->errorString());
    }
}

int64_t Downloader::getContentLength(Job* job, const QUrl &url,
        HWND parentWindow)
{
    return downloadWin(job, url, L"HEAD", 0, 0, 0, parentWindow, 0, false,
            QCryptographicHash::Sha1);
}

QTemporaryFile* Downloader::download(Job* job, const QUrl &url, QString* sha1,
        QCryptographicHash::Algorithm alg,
        bool useCache)
{
    QTemporaryFile* file = new QTemporaryFile();

    if (file->open()) {
        download(job, url, file, sha1, alg, useCache);
        file->close();

        if (job->isCancelled() || !job->getErrorMessage().isEmpty()) {
            delete file;
            file = 0;
        }
    } else {
        job->setErrorMessage(QString(QObject::tr("Error opening file: %1")).
                arg(file->fileName()));
        delete file;
        file = 0;
        job->complete();
    }

    return file;
}
