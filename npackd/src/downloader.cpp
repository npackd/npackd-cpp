#include <cmath>

#include <windows.h>
#include <wininet.h>

#include <zlib.h>

#include <QObject>
#include <QWaitCondition>
#include <QCryptographicHash>
#include <QLoggingCategory>

#include "downloader.h"
#include "job.h"
#include "wpmutils.h"

HWND defaultPasswordWindow = nullptr;
std::mutex loginDialogMutex;

DWORD __stdcall myInternetAuthNotifyCallback(DWORD_PTR /* dwContext */,
        DWORD dwReturn, LPVOID /* lpReserved */) {
    qCDebug(npackd) << "myInternetAuthNotifyCallback" << dwReturn;
    return 0;
}

int64_t Downloader::downloadWin(Job* job, const Request& request,
        Downloader::Response* response)
{
    QUrl url = request.url;
    QString verb = request.httpMethod;
    QFile* file = request.file;
    QString* mime = &response->mimeType;
    QString* contentDisposition = &response->contentDisposition;
    HWND parentWindow = defaultPasswordWindow;
    QString* sha1 = &response->hashSum;
    bool useCache = request.useCache;
    QCryptographicHash::Algorithm alg = request.alg;
    bool keepConnection = request.keepConnection;
    bool interactive = request.interactive;

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

    HINTERNET internet = InternetOpenW(WPMUtils::toLPWSTR(agent),
            INTERNET_OPEN_TYPE_PRECONFIG,
            nullptr, nullptr, 0);

    if (internet == nullptr) {
        QString errMsg;
        WPMUtils::formatMessage(GetLastError(), &errMsg);
        job->setErrorMessage(errMsg);
    }

    if (job->shouldProceed()) {
        // change the timeout to 5 minutes
        DWORD rec_timeout = static_cast<DWORD>(request.timeout) * 1000;
        InternetSetOption(internet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));
        InternetSetOption(internet, INTERNET_OPTION_SEND_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));

        // enable automatic gzip decoding
#ifndef INTERNET_OPTION_HTTP_DECODING
        const DWORD INTERNET_OPTION_HTTP_DECODING = 65;
#endif
        BOOL b = TRUE;
        InternetSetOption(internet, INTERNET_OPTION_HTTP_DECODING,
                &b, sizeof(b));

        job->setProgress(0.01);
    }

    // here "internet" cannot be 0, but we add the comparison to silence
    // Coverity
    HINTERNET hConnectHandle = nullptr;
    if (job->shouldProceed() && internet != nullptr) {
        INTERNET_PORT port = static_cast<INTERNET_PORT>(
                url.port(url.scheme() == "https" ?
                INTERNET_DEFAULT_HTTPS_PORT: INTERNET_DEFAULT_HTTP_PORT));
        hConnectHandle = InternetConnectW(internet,
                WPMUtils::toLPWSTR(server), port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);

        if (hConnectHandle == nullptr) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
        }
    }

    if (job->shouldProceed()) {
        if (!request.user.isEmpty()) {
            setStringOption(hConnectHandle, INTERNET_OPTION_USERNAME,
                    request.user);

            if (!request.password.isEmpty()) {
                setStringOption(hConnectHandle, INTERNET_OPTION_PASSWORD,
                        request.password);
            }
        }
    }

    if (job->shouldProceed()) {
        if (!request.proxyUser.isEmpty()) {
            setStringOption(hConnectHandle, INTERNET_OPTION_PROXY_USERNAME,
                    request.proxyUser);

            if (!request.proxyPassword.isEmpty()) {
                setStringOption(hConnectHandle, INTERNET_OPTION_PROXY_PASSWORD,
                        request.proxyPassword);
            }
        }
    }

    // flags: http://msdn.microsoft.com/en-us/library/aa383661(v=vs.85).aspx
    // We support accepting any mime file type since this is a simple download
    // of a file
    //
    // hConnectHandle is only checked here to silence Coverity
    HINTERNET hResourceHandle = nullptr;
    if (job->shouldProceed() && hConnectHandle != nullptr) {
        LPCTSTR ppszAcceptTypes[2];
        ppszAcceptTypes[0] = L"*/*";
        ppszAcceptTypes[1] = nullptr;
        DWORD flags = (url.scheme() == "https" ? INTERNET_FLAG_SECURE : 0);
        if (keepConnection)
            flags |= INTERNET_FLAG_KEEP_CONNECTION;
        if (!request.useInternet)
            flags |= INTERNET_FLAG_FROM_CACHE;
        flags |= INTERNET_FLAG_RESYNCHRONIZE;
        if (!useCache)
            flags |= INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_PRAGMA_NOCACHE |
                    INTERNET_FLAG_RELOAD;
        hResourceHandle = HttpOpenRequestW(hConnectHandle,
                WPMUtils::toLPWSTR(verb),
                WPMUtils::toLPWSTR(resource),
                nullptr, nullptr, ppszAcceptTypes,
                flags, 0);
        if (hResourceHandle == nullptr) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
        }
    }

    int64_t contentLength = -1;

    if (hResourceHandle != nullptr && hConnectHandle != nullptr) {
        if (job->shouldProceed()) {
            // do not check for errors here
            HttpAddRequestHeadersW(hResourceHandle,
                    L"Accept-Encoding: gzip, deflate",
                    static_cast<DWORD>(-1),
                    HTTP_ADDREQ_FLAG_ADD);
        }

        int callNumber = 0;
        while (job->shouldProceed()) {
            DWORD sendRequestError = 0;

            // the following call uses NULL for headers in case there are no headers
            // because Windows 2003 generates the error 12150 otherwise
            if (!HttpSendRequestW(hResourceHandle,
                    request.headers.length() == 0 ? nullptr : WPMUtils::toLPWSTR(request.headers),
                    static_cast<DWORD>(-1),
                    request.postData.length() == 0 ? nullptr : const_cast<char*>(request.postData.data()),
                    static_cast<DWORD>(request.postData.length()))) {
                sendRequestError = GetLastError();
            }

            qCDebug(npackd) << "Downloader::downloadWin callNumber="
                    << callNumber << ", sendRequestError="
                    << sendRequestError;

            // both calls to InternetErrorDlg should be enclosed by one
            // mutex, so that only one dialog will be shown
            loginDialogMutex.lock();

            void* p = nullptr;

            if (job->shouldProceed()) {
                if (sendRequestError == 0) {
                    // convert HTTP status codes into errors
                    // (FLAGS_ERROR_UI_FILTER_FOR_ERRORS)

                    DWORD flags = FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
                            FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS |
                            FLAGS_ERROR_UI_FLAGS_GENERATE_DATA;
                    if (!interactive || parentWindow == 0)
                        flags |= FLAGS_ERROR_UI_FLAGS_NO_UI;
                    sendRequestError = InternetErrorDlg(parentWindow,
                           hResourceHandle, sendRequestError,
                           flags, &p);
                }
            }

            if (job->shouldProceed()) {
                switch (sendRequestError) {
                    case ERROR_HTTP_REDIRECT_NEEDS_CONFIRMATION:
                    case ERROR_INTERNET_BAD_AUTO_PROXY_SCRIPT:
                    case ERROR_INTERNET_CHG_POST_IS_NON_SECURE:
                    case ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED:
                    case ERROR_INTERNET_HTTP_TO_HTTPS_ON_REDIR:
                    case ERROR_INTERNET_HTTPS_TO_HTTP_ON_REDIR:
                    case ERROR_INTERNET_HTTPS_HTTP_SUBMIT_REDIR:
                    case ERROR_INTERNET_INCORRECT_PASSWORD:
                    case ERROR_INTERNET_INVALID_CA:
                    case ERROR_INTERNET_MIXED_SECURITY:
                    case ERROR_INTERNET_POST_IS_NON_SECURE:
                    case ERROR_INTERNET_SEC_CERT_CN_INVALID:
                    case ERROR_INTERNET_SEC_CERT_ERRORS:
                    case ERROR_INTERNET_SEC_CERT_DATE_INVALID:
                    case ERROR_INTERNET_SEC_CERT_REV_FAILED:
                    case ERROR_INTERNET_SEC_CERT_REVOKED:
                    case ERROR_INTERNET_UNABLE_TO_DOWNLOAD_SCRIPT:
                    {
                        DWORD flags = FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS |
                                FLAGS_ERROR_UI_FLAGS_GENERATE_DATA;
                        if (!interactive || parentWindow == 0)
                            flags |= FLAGS_ERROR_UI_FLAGS_NO_UI;
                        sendRequestError = InternetErrorDlg(parentWindow,
                               hResourceHandle, sendRequestError,
                               flags, &p);
                    }
                }
            }

            loginDialogMutex.unlock();

            if (job->shouldProceed()) {
                if (sendRequestError == ERROR_SUCCESS) {
                    break;
                } else if (sendRequestError == ERROR_INTERNET_FORCE_RETRY) {
                    // nothing
                } else if (sendRequestError == ERROR_CANCELLED) {
                    QString error;
                    WPMUtils::formatMessage(sendRequestError, &error);
                    job->setErrorMessage(error);
                }
            }

            if (!job->shouldProceed())
                break;

            // read all the data before re-sending the request
            if (!request.ignoreContent) {
                char smallBuffer[4 * 1024];
                while (true) {
                    DWORD read;
                    if (!InternetReadFile(hResourceHandle, &smallBuffer,
                            sizeof(smallBuffer), &read)) {
                        break;
                    }

                    if (read == 0)
                        break;
                }
            }

            callNumber++;

            if (callNumber > 2) {
                job->setErrorMessage(QObject::tr("Too many retries"));
                break;
            }
        }; // while (job->shouldProceed())

        if (job->shouldProceed()) {
            DWORD dwStatus, dwStatusSize = sizeof(dwStatus);

            // http://msdn.microsoft.com/en-us/library/aa384220(v=vs.85).aspx
            if (!HttpQueryInfo(hResourceHandle, HTTP_QUERY_FLAG_NUMBER |
                    HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, nullptr)) {
                QString errMsg;
                WPMUtils::formatMessage(GetLastError(), &errMsg);
                job->setErrorMessage(errMsg);
            } else {
                // 2XX
                if (dwStatus / 100 != 2) {
                    job->setErrorMessage(QString(
                            QObject::tr("HTTP status code %1")).arg(dwStatus));
                }
            }
        }

        if (job->shouldProceed()) {
            job->setProgress(0.03);
            job->setTitle(initialTitle + " / " + QObject::tr("Downloading"));
        }

        // MIME type
        if (job->shouldProceed()) {
            if (mime) {
                WCHAR mimeBuffer[1024];
                DWORD bufferLength = sizeof(mimeBuffer);
                DWORD index = 0;
                if (!HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CONTENT_TYPE,
                        &mimeBuffer, &bufferLength, &index)) {
                    *mime = "application/octet-stream";
                } else {
                    *mime = QString::fromWCharArray(
                            mimeBuffer, bufferLength / 2);
                }
            }
        }

        bool gzip = false;

        // Content-Encoding
        if (job->shouldProceed()) {
            WCHAR contentEncodingBuffer[1024];
            DWORD bufferLength = sizeof(contentEncodingBuffer);
            DWORD index = 0;
            if (HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CONTENT_ENCODING,
                    &contentEncodingBuffer, &bufferLength, &index)) {
                QString contentEncoding = QString::fromWCharArray(
                        contentEncodingBuffer, bufferLength / 2);
                gzip = contentEncoding == "gzip" || contentEncoding == "deflate";
            }

            job->setProgress(0.04);
        }

        // Content-Disposition
        if (job->shouldProceed()) {
            if (contentDisposition) {
                WCHAR cdBuffer[1024];
                wcscpy(cdBuffer, L"Content-Disposition");
                DWORD bufferLength = sizeof(cdBuffer);
                DWORD index = 0;
                if (HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CUSTOM,
                        &cdBuffer, &bufferLength, &index)) {
                    *contentDisposition = QString::fromWCharArray(
                            cdBuffer, bufferLength / 2);
                }
            }
        }

        // content length
        if (job->shouldProceed()) {
            WCHAR contentLengthBuffer[100];
            DWORD bufferLength = sizeof(contentLengthBuffer);
            DWORD index = 0;
            if (HttpQueryInfoW(hResourceHandle, HTTP_QUERY_CONTENT_LENGTH,
                    contentLengthBuffer, &bufferLength, &index)) {
                QString s = QString::fromWCharArray(
                        contentLengthBuffer, bufferLength / 2);
                bool ok;
                contentLength = s.toLongLong(&ok, 10);
                if (!ok)
                    contentLength = 0;
            }

            job->setProgress(0.05);
        }

        if (job->shouldProceed()) {
            if (!request.ignoreContent) {
                Job* sub = job->newSubJob(0.95, QObject::tr("Reading the data"));
                readData(sub, hResourceHandle, file, sha1, gzip, contentLength, alg);
                if (!sub->getErrorMessage().isEmpty())
                    job->setErrorMessage(sub->getErrorMessage());
            } else {
                job->setProgress(job->getProgress() + 0.95);
            }
        }

        InternetCloseHandle(hResourceHandle);
    }

    if (hConnectHandle)
        InternetCloseHandle(hConnectHandle);
    if (internet)
        InternetCloseHandle(internet);

    if (job->shouldProceed())
        job->setProgress(1);

    job->setTitle(initialTitle);

    job->complete();

    qCDebug(npackd) << request.url << job->getErrorMessage() << contentLength;

    return contentLength;
}

QString Downloader::setStringOption(HINTERNET hInternet, DWORD dwOption,
        const QString& value)
{
    QString result;
    if (!InternetSetOptionW(hInternet,
            dwOption, WPMUtils::toLPWSTR(value),
            static_cast<DWORD>(value.length() + 1))) {
        WPMUtils::formatMessage(GetLastError(), &result);
    }
    return result;
}

QString Downloader::inputPassword(HINTERNET hConnectHandle,
        DWORD dwStatus)
{
    QString result;

    QString username, password;
    if (dwStatus == HTTP_STATUS_PROXY_AUTH_REQ) {
        WPMUtils::writeln("\r\n" +
                QObject::tr("The HTTP proxy requires authentication."));
        WPMUtils::outputTextConsole(QObject::tr("Username") + ": ");
        username = WPMUtils::inputTextConsole();
        WPMUtils::outputTextConsole(QObject::tr("Password") + ": ");
        password = WPMUtils::inputPasswordConsole();

        if (result.isEmpty()) {
            result = setStringOption(hConnectHandle,
                INTERNET_OPTION_PROXY_USERNAME, username);
        }

        if (result.isEmpty()) {
            result = setStringOption(hConnectHandle,
                INTERNET_OPTION_PROXY_PASSWORD, password);
        }
    } else if (dwStatus == HTTP_STATUS_DENIED) {
        WPMUtils::writeln("\r\n" +
                QObject::tr("The HTTP server requires authentication.")
                );
        WPMUtils::outputTextConsole(QObject::tr("Username") + ": ");
        username = WPMUtils::inputTextConsole();
        WPMUtils::outputTextConsole(QObject::tr("Password") + ": ");
        password = WPMUtils::inputPasswordConsole();

        if (result.isEmpty()) {
            result = setStringOption(hConnectHandle,
                INTERNET_OPTION_USERNAME, username);
        }

        if (result.isEmpty()) {
            result = setStringOption(hConnectHandle,
                INTERNET_OPTION_PASSWORD, password);
        }
    } else {
        result = QString(QObject::tr("Cannot handle HTTP status code %1")).
                arg(dwStatus);
    }

    return result;
}

bool Downloader::internetReadFileFully(HINTERNET resourceHandle,
        PVOID buffer, DWORD bufferSize, PDWORD bufferLength)
{
    DWORD alreadyRead = 0;
    bool result;
    while (true) {
        DWORD len;
        result = InternetReadFile(resourceHandle,
                static_cast<uint8_t*>(buffer) + alreadyRead,
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

            d_stream.zalloc = nullptr;
            d_stream.zfree = nullptr;
            d_stream.opaque = nullptr;

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
                    hash.addData(reinterpret_cast<char*>(buffer2),
                            buffer2Size - static_cast<int>(d_stream.avail_out));

                file->write(reinterpret_cast<char*>(buffer2),
                        buffer2Size - static_cast<int>(d_stream.avail_out));
            }
        } while (d_stream.avail_out == 0);

        if (!job->getErrorMessage().isEmpty())
            break;

        alreadyRead += bufferLength;
        if (contentLength > 0) {
            job->setProgress((static_cast<double>(alreadyRead)) / contentLength);
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

    if (sha1 && job->shouldProceed())
        *sha1 = hash.result().toHex().toLower();

// out:
    delete[] buffer;
    delete[] buffer2;

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();
}

void Downloader::readDataFlat(Job* job, HINTERNET hResourceHandle, QFile* file,
        QString* sha1, int64_t contentLength, QCryptographicHash::Algorithm alg)
{
    qCDebug(npackd) << "Downloader::readDataFlat";

    QString initialTitle = job->getTitle();

    // download/compute SHA1 loop
    QCryptographicHash hash(alg);
    const int bufferSize = 512 * 1024;
    unsigned char* buffer = new unsigned char[bufferSize];

    int64_t alreadyRead = 0;
    DWORD bufferLength;
    do {
        if (!InternetReadFile(hResourceHandle, buffer,
                bufferSize, &bufferLength)) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
            break;
        }

        qCDebug(npackd) <<
                "Downloader::readDataFlat InternetReadFile bufferLength=" <<
                bufferLength;

        if (bufferLength == 0)
            break;

        // update SHA1 if necessary
        if (sha1)
            hash.addData(reinterpret_cast<char*>(buffer),
                    static_cast<int>(bufferLength));

        if (file) {
            if (file->write(reinterpret_cast<char*>(buffer), bufferLength) < 0) {
                job->setErrorMessage(file->errorString());
                break;
            }
        }

        alreadyRead += bufferLength;
        if (contentLength > 0) {
            job->setProgress((static_cast<double>(alreadyRead)) /
                    contentLength);
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

    if (job->shouldProceed())
        job->setProgress(1);

    if (sha1 && job->shouldProceed())
        *sha1 = hash.result().toHex().toLower();

    delete[] buffer;

    job->complete();
}

void Downloader::readData(Job* job, HINTERNET hResourceHandle, QFile* file,
        QString* sha1, bool gzip, int64_t contentLength,
        QCryptographicHash::Algorithm alg)
{
    if (gzip && file)
        readDataGZip(job, hResourceHandle, file, sha1, contentLength, alg);
    else
        readDataFlat(job, hResourceHandle, file, sha1, contentLength, alg);
}

void Downloader::copyFile(Job* job, const QString& source, QFile* file,
         QString* sha1, QCryptographicHash::Algorithm alg) {
    QFile srcFile(source);
    if (!srcFile.open(QFile::ReadOnly)) {
        job->setErrorMessage(QObject::tr("Error opening file: %1").
                arg(source));
    } else {
        qint64 srcSize = srcFile.size();
        const int SZ = 8192;
        char* data = new char[SZ];

        qint64 progress = 0;
        QCryptographicHash crypto(alg);
        while(true) {
            qint64 c = srcFile.read(data, SZ);
            if (c <= 0)
                break;

            if (sha1)
                crypto.addData(data, static_cast<int>(c));
            if (file)
                file->write(data, c);

            progress += c;
            if (srcSize != 0)
                job->setProgress((static_cast<double>(progress)) / srcSize);
        }

        if (sha1)
            *sha1 = crypto.result().toHex().toLower();

        srcFile.close();
        delete[] data;
    }
    job->complete();
}

Downloader::Response Downloader::download(Job *job,
        const Downloader::Request &request)
{
    Downloader::Response r;

    QString* sha1 = request.hashSum ? &r.hashSum : nullptr;
    if (request.url.scheme() == "https" || request.url.scheme() == "http")
        downloadWin(job, request, &r);
    else if (request.url.toString().startsWith("data:image/png;base64,")) {
        if (request.file) {
            QString dataURL_ = request.url.toString().mid(22);
            QByteArray ba = QByteArray::fromBase64(dataURL_.toLatin1());
            if (request.file->write(ba) < 0)
                job->setErrorMessage(request.file->errorString());
        }
        r.mimeType = "image/png";
        job->complete();
    } else if (request.url.scheme() == "file") {
        QString localFile = request.url.toLocalFile();
        QFileInfo fi(localFile);
        if (fi.isAbsolute())
            copyFile(job, localFile, request.file, sha1, request.alg);
        else {
            job->setErrorMessage(
                    QObject::tr("Cannot download a file from a relative path %1").
                    arg(localFile));
            job->complete();
        }
        r.mimeType = "image/png";
    } else {
        job->setErrorMessage(QObject::tr("Unsupported URL scheme: %1").
                arg(request.url.toDisplayString()));
        job->complete();
    }

    return r;
}

int64_t Downloader::getContentLength(Job* job, const QUrl &url,
        HWND parentWindow)
{
    int64_t result = -1;
    if (url.scheme() == "file") {
        QString filename = url.toLocalFile();
        QFileInfo fi(filename);
        if (fi.isAbsolute()) {
            result = fi.size();
        } else {
            job->setErrorMessage(QObject::tr("Cannot process relative file name %1").
                    arg(filename));
        }
        job->complete();
    } else {
        Request req(url);
        req.httpMethod = "HEAD";
        req.parentWindow = parentWindow;
        req.useCache = true;
        req.keepConnection = false;
        req.timeout = 15;

        Job* sub = job->newSubJob(1, QObject::tr("Using the HEAD HTTP method"));
        Response resp;
        result = downloadWin(sub, req, &resp);

        if (!sub->getErrorMessage().isEmpty()) {
            Request req2(url);
            req2.parentWindow = parentWindow;
            req2.useCache = true;
            req2.keepConnection = false;
            req2.timeout = 15;
            req2.ignoreContent = true;

            Response resp2;
            Job* sub2 = job->newSubJob(1 - job->getProgress(),
                    QObject::tr("Using the GET HTTP method"));
            result = downloadWin(sub2, req2, &resp2);
            if (!sub2->getErrorMessage().isEmpty())
                job->setErrorMessage(sub2->getErrorMessage());
        }
    }

    return result;
}

QTemporaryFile* Downloader::downloadToTemporary(Job* job,
        const Downloader::Request &request)
{
    QTemporaryFile* file = new QTemporaryFile();
    Downloader::Request r2(request);
    r2.file = file;

    if (file->open()) {
        download(job, r2);
        file->close();

        if (!job->shouldProceed()) {
            delete file;
            file = nullptr;
        }
    } else {
        job->setErrorMessage(QString(QObject::tr("Error opening file: %1")).
                arg(file->fileName()));
        delete file;
        file = nullptr;
        job->complete();
    }

    return file;
}

