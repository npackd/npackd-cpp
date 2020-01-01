#include <windows.h>
#include <wininet.h>

#include <QObject>

#include "asyncdownloader.h"
#include "wpmutils.h"

#define BUFFER_LEN  4096
#define ERR_MSG_LEN 512

#define REQ_STATE_SEND_REQ             0
#define REQ_STATE_SEND_REQ_WITH_BODY   1
#define REQ_STATE_POST_GET_DATA        2
#define REQ_STATE_POST_SEND_DATA       3
#define REQ_STATE_POST_COMPLETE        4
#define REQ_STATE_RESPONSE_RECV_DATA   5
#define REQ_STATE_RESPONSE_WRITE_DATA  6
#define REQ_STATE_COMPLETE             7

#define SPIN_COUNT 4000

//AsyncDownloader::AsyncDownloader()
//{
    // TODO: gzip, authentication, progress, POST, SHA1
//}

int64_t AsyncDownloader::downloadWin2(Job* job,
        const Downloader::Request& request, Downloader::Response *response)
{
    QUrl url = request.url;
    QString* mime = &response->mimeType;
    QString* contentDisposition = &response->contentDisposition;
    HWND parentWindow = defaultPasswordWindow;
    QString* sha1 = &response->hashSum;
    bool useCache = request.useCache;
    QCryptographicHash::Algorithm alg = request.alg;
    bool keepConnection = request.keepConnection;
    bool interactive = request.interactive;

    QString initialTitle = job->getTitle();

    job->setTitle(initialTitle + " / " + tr("Connecting"));

    if (sha1)
        sha1->clear();

    QString server = url.host();

    QString agent("Npackd/");
    agent.append(NPACKD_VERSION);

    agent += " (compatible; MSIE 9.0)";

    HINTERNET SessionHandle = InternetOpenW(WPMUtils::toLPWSTR(agent),
            INTERNET_OPEN_TYPE_PRECONFIG,
            nullptr, nullptr, INTERNET_FLAG_ASYNC);

    if (SessionHandle == nullptr) {
        QString errMsg;
        WPMUtils::formatMessage(GetLastError(), &errMsg);
        job->setErrorMessage(errMsg);
    }

    if (job->shouldProceed()) {
/*
        // Note: This only works for INTERNET_OPTION_RECEIVE_TIMEOUT.
        // The connect timeout (INTERNET_OPTION_CONNECT_TIMEOUT) and
        // Send Timeout (INTERNET_OPTION_SEND_TIMEOUT) are broken with Win
        // Inet in synchronous mode.
        DWORD rec_timeout = static_cast<DWORD>(request.timeout) * 1000;
        InternetSetOption(internet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));
        InternetSetOption(internet, INTERNET_OPTION_SEND_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));

         InternetSetOption(internet, INTERNET_OPTION_CONNECT_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));
/ *
         InternetSetOption(internet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));
        InternetSetOption(internet, INTERNET_OPTION_DATA_SEND_TIMEOUT,
                &rec_timeout, sizeof(rec_timeout));


*        rec_timeout = 1;
        InternetSetOption(internet, INTERNET_OPTION_CONNECT_RETRIES,
                &rec_timeout, sizeof(rec_timeout));

*
 * INTERNET_OPTION_CONNECT_TIMEOUT,
(void *)&var, sizeof(var));
b = InternetSetOption(fSession, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,
(void *)&var, sizeof(var));
b = InternetSetOption(fSession, INTERNET_OPTION_DATA_SEND_TIMEOUT,
(void *)&var, sizeof(var));
b = InternetSetOption(fSession, INTERNET_OPTION_SEND_TIMEOUT, (void
*)&var, sizeof(var));
b = InternetSetOption(fSession, INTERNET_OPTION_RECEIVE_TIMEOUT, (void
*)&var, sizeof(var));
var = 4;  // Retries
b = InternetSetOption(fSession, INTERNET_OPTION_CONNECT_RETRIES, (void
*)&var, sizeof(var));
*
 */
        // enable automatic gzip decoding
#ifndef INTERNET_OPTION_HTTP_DECODING
        const DWORD INTERNET_OPTION_HTTP_DECODING = 65;
#endif
        BOOL b = TRUE;
        InternetSetOption(SessionHandle, INTERNET_OPTION_HTTP_DECODING,
                &b, sizeof(b));

        job->setProgress(0.01);
    }

    DWORD Error = 0;

    PREQUEST_CONTEXT ReqContext = nullptr;

    // Callback function
    INTERNET_STATUS_CALLBACK CallbackPointer;



    if (SessionHandle == nullptr)
    {
        LogInetError(GetLastError(), L"InternetOpen");
        goto Exit;
    }


    // Set the status callback for the handle to the Callback function
    CallbackPointer = InternetSetStatusCallback(SessionHandle,
                                                (INTERNET_STATUS_CALLBACK)CallBack);

    if (CallbackPointer == INTERNET_INVALID_STATUS_CALLBACK)
    {
        qCDebug(npackd) << "InternetSetStatusCallback failed with INTERNET_INVALID_STATUS_CALLBACK";
        goto Exit;
    }



    // Initialize the ReqContext to be used in the asynchronous calls
    Error = AsyncDownloader::AllocateAndInitializeRequestContext(
            job, SessionHandle, &ReqContext, request);
    ReqContext->request = &request;
    ReqContext->response = response;

    if (Error != ERROR_SUCCESS)
    {
        qCDebug(npackd) << "AllocateAndInitializeRequestContext failed with error" << Error;
        goto Exit;
    }

    //
    // Send out request and receive response
    //

    ProcessRequest(ReqContext, ERROR_SUCCESS);


    //
    // Wait for request completion or timeout
    //

    WaitForRequestCompletion(ReqContext,
            static_cast<DWORD>(request.timeout) * 1000);


Exit:

    // Clean up the allocated resources
    CleanUpRequestContext(ReqContext);

    if (SessionHandle) {
        //
        // Remove the callback from the SessionHandle since
        // we don't want the closing notification
        //
        (void)InternetSetStatusCallback(SessionHandle, NULL);

        //
        // Call InternetCloseHandle and do not wait for the closing notification
        // in the callback function
        //
        (void)InternetCloseHandle(SessionHandle);
    }

    return 0;
}

void CALLBACK CallBack(HINTERNET hInternet, DWORD_PTR dwContext,
        DWORD dwInternetStatus, LPVOID lpvStatusInformation,
        DWORD dwStatusInformationLength)
{
    InternetCookieHistory cookieHistory;
    PREQUEST_CONTEXT ReqContext = (PREQUEST_CONTEXT)dwContext;

    UNREFERENCED_PARAMETER(dwStatusInformationLength);

    qCDebug(npackd) << "Callback Received for Handle" << hInternet;

    switch(dwInternetStatus)
    {
        case INTERNET_STATUS_COOKIE_SENT:
            qCDebug(npackd) << "Status: Cookie found and will be sent with request";
            break;

        case INTERNET_STATUS_COOKIE_RECEIVED:
            qCDebug(npackd) << "Status: Cookie Received";
            break;

        case INTERNET_STATUS_COOKIE_HISTORY:

            qCDebug(npackd) << "Status: Cookie History";

            // ASYNC_ASSERT(lpvStatusInformation);
            // ASYNC_ASSERT(dwStatusInformationLength == sizeof(InternetCookieHistory));

            cookieHistory = *((InternetCookieHistory*)lpvStatusInformation);

            if(cookieHistory.fAccepted)
            {
                qCDebug(npackd) << "Cookie Accepted";
            }
            if(cookieHistory.fLeashed)
            {
                qCDebug(npackd) << "Cookie Leashed";
            }
            if(cookieHistory.fDowngraded)
            {
                qCDebug(npackd) << "Cookie Downgraded";
            }
            if(cookieHistory.fRejected)
            {
                qCDebug(npackd) << "Cookie Rejected";
            }


            break;

        case INTERNET_STATUS_CLOSING_CONNECTION:
            qCDebug(npackd) << "Status: Closing Connection";
            break;

        case INTERNET_STATUS_CONNECTED_TO_SERVER:
            qCDebug(npackd) << "Status: Connected to Server";
            break;

        case INTERNET_STATUS_CONNECTING_TO_SERVER:
            qCDebug(npackd) << "Status: Connecting to Server";
            break;

        case INTERNET_STATUS_CONNECTION_CLOSED:
            qCDebug(npackd) << "Status: Connection Closed";
            break;

        case INTERNET_STATUS_HANDLE_CLOSING:
            qCDebug(npackd) << "Status: Handle Closing";

            //
            // Signal the cleanup routine that it is
            // safe to cleanup the request context
            //

            // ASYNC_ASSERT(ReqContext);
            SetEvent(ReqContext->CleanUpEvent);

            break;

        case INTERNET_STATUS_HANDLE_CREATED:
            // ASYNC_ASSERT(lpvStatusInformation);
            qCDebug(npackd) << "Handle %x created"
            <<                  ((LPINTERNET_ASYNC_RESULT)lpvStatusInformation)->dwResult;

            break;

        case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
            qCDebug(npackd) << "Status: Intermediate response";
            break;

        case INTERNET_STATUS_RECEIVING_RESPONSE:
            qCDebug(npackd) << "Status: Receiving Response";
            break;

        case INTERNET_STATUS_RESPONSE_RECEIVED:
            // ASYNC_ASSERT(lpvStatusInformation);
            // ASYNC_ASSERT(dwStatusInformationLength == sizeof(DWORD));

            qCDebug(npackd) << "Status: Response Received (%d Bytes)"<<
                *((LPDWORD)lpvStatusInformation);

            break;

        case INTERNET_STATUS_REDIRECT:
            qCDebug(npackd) << "Status: Redirect";
            break;

        case INTERNET_STATUS_REQUEST_COMPLETE:
            qCDebug(npackd) << "Status: Request complete";

            // ASYNC_ASSERT(lpvStatusInformation);

            AsyncDownloader::ProcessRequest(ReqContext, ((LPINTERNET_ASYNC_RESULT)lpvStatusInformation)->dwError);

            break;

        case INTERNET_STATUS_REQUEST_SENT:
            // ASYNC_ASSERT(lpvStatusInformation);
            // ASYNC_ASSERT(dwStatusInformationLength == sizeof(DWORD));

            qCDebug(npackd) << "Status: Request sent (%d Bytes)"<<
                    *((LPDWORD)lpvStatusInformation);
            break;

        case INTERNET_STATUS_DETECTING_PROXY:
            qCDebug(npackd) << "Status: Detecting Proxy";
            break;

        case INTERNET_STATUS_RESOLVING_NAME:
            qCDebug(npackd) << "Status: Resolving Name";
            break;

        case INTERNET_STATUS_NAME_RESOLVED:
            qCDebug(npackd) << "Status: Name Resolved";
            break;

        case INTERNET_STATUS_SENDING_REQUEST:
            qCDebug(npackd) << "Status: Sending request";
            break;

        case INTERNET_STATUS_STATE_CHANGE:
            qCDebug(npackd) << "Status: State Change";
            break;

        case INTERNET_STATUS_P3P_HEADER:
            qCDebug(npackd) << "Status: Received P3P header";
            break;

        default:
            qCDebug(npackd) << "Status: Unknown (%d)"<< dwInternetStatus;
            break;
    }

    return;
}


void AsyncDownloader::ProcessRequest(PREQUEST_CONTEXT ReqContext, DWORD Error)
{
    BOOL Eof = FALSE;

    // ASYNC_ASSERT(ReqContext);

    while (Error == ERROR_SUCCESS && ReqContext->State != REQ_STATE_COMPLETE) {
        switch (ReqContext->State) {
            case REQ_STATE_SEND_REQ:
                ReqContext->State = REQ_STATE_RESPONSE_RECV_DATA;
                Error = SendRequest(ReqContext);
                break;
            case REQ_STATE_SEND_REQ_WITH_BODY:
                ReqContext->State = REQ_STATE_POST_GET_DATA;
                Error = SendRequestWithBody(ReqContext);
                break;
            case REQ_STATE_POST_GET_DATA:
                ReqContext->State = REQ_STATE_POST_SEND_DATA;
                Error = GetDataToPost(ReqContext);
                break;
            case REQ_STATE_POST_SEND_DATA:
                ReqContext->State = REQ_STATE_POST_GET_DATA;
                Error = PostDataToServer(ReqContext, &Eof);
                if(Eof) {
                    // ASYNC_ASSERT(Error == ERROR_SUCCESS);
                    ReqContext->State = REQ_STATE_POST_COMPLETE;
                }
                break;
            case REQ_STATE_POST_COMPLETE:
                ReqContext->State = REQ_STATE_RESPONSE_RECV_DATA;
                Error = CompleteRequest(ReqContext);
                break;
            case REQ_STATE_RESPONSE_RECV_DATA:
                ReqContext->State = REQ_STATE_RESPONSE_WRITE_DATA;
                Error = RecvResponseData(ReqContext);
                break;
            case REQ_STATE_RESPONSE_WRITE_DATA:
                ReqContext->State = REQ_STATE_RESPONSE_RECV_DATA;
                Error = WriteResponseData(ReqContext, &Eof);
                if (Eof) {
                    // ASYNC_ASSERT(Error == ERROR_SUCCESS);
                    ReqContext->State = REQ_STATE_COMPLETE;
                }
                break;
            default:
                // ASYNC_ASSERT(FALSE);
                break;
        }
    }

    if (Error != ERROR_IO_PENDING) {
        //
        // Everything has been procesed or has failed.
        // In either case, the signal processing has
        // completed
        //
        SetEvent(ReqContext->CompletionEvent);
    }
}

DWORD AsyncDownloader::SendRequest(PREQUEST_CONTEXT ReqContext)
{
    BOOL Success;
    DWORD Error = ERROR_SUCCESS;


    Success = AcquireRequestHandle(ReqContext);
    if(!Success)
    {
        Error = ERROR_OPERATION_ABORTED;
        goto Exit;
    }

    // the following call uses NULL for headers in case there are no headers
    // because Windows 2003 generates the error 12150 otherwise
    Success = HttpSendRequest(ReqContext->RequestHandle,
            ReqContext->request->headers.length() == 0 ? nullptr :
            WPMUtils::toLPWSTR(ReqContext->request->headers),                   // do not provide additional Headers
            0,                      // dwHeadersLength
            NULL,                   // Do not send any data
            0);                     // dwOptionalLength

    ReleaseRequestHandle(ReqContext);

    if (!Success)
    {
        Error = GetLastError();

        if(Error != ERROR_IO_PENDING)
        {
            LogInetError(Error, L"HttpSendRequest");
        }
        goto Exit;
    }

Exit:

    return Error;
}

DWORD AsyncDownloader::SendRequestWithBody(PREQUEST_CONTEXT ReqContext)
{
    BOOL Success;
    INTERNET_BUFFERS BuffersIn;
    DWORD Error = ERROR_SUCCESS;

    //
    // HttpSendRequest can also be used also to post data to a server,
    // to do so, the data should be provided using the lpOptional
    // parameter and it's size on dwOptionalLength.
    // Here we decided to depict the use of HttpSendRequestEx function.
    //

    //Prepare the Buffers to be passed to HttpSendRequestEx
    ZeroMemory(&BuffersIn, sizeof(INTERNET_BUFFERS));
    BuffersIn.dwStructSize = sizeof(INTERNET_BUFFERS);
    BuffersIn.lpvBuffer = nullptr;
    BuffersIn.dwBufferLength = 0;
    BuffersIn.dwBufferTotal = ReqContext->FileSize; // content-length of data to post


    Success = AcquireRequestHandle(ReqContext);
    if(!Success)
    {
        Error = ERROR_OPERATION_ABORTED;
        goto Exit;
    }

    // TODO: headers

    Success = HttpSendRequestEx(ReqContext->RequestHandle,
                                &BuffersIn,
                                nullptr,                 // Do not use output buffers
                                0,                    // dwFlags reserved
                                (DWORD_PTR)ReqContext);

    ReleaseRequestHandle(ReqContext);

    if (!Success)
    {
        Error = GetLastError();

        if(Error != ERROR_IO_PENDING)
        {
            LogInetError(Error, L"HttpSendRequestEx");
        }

        goto Exit;
    }

Exit:

    return Error;
}

DWORD AsyncDownloader::GetDataToPost(PREQUEST_CONTEXT ReqContext)
{
    DWORD Error = ERROR_SUCCESS;
//    BOOL Success;


    //
    //
    // ReadFile is done inline here assuming that it will return quickly
    // I.E. the file is on disk
    //
    // If you plan to do blocking/intensive operations they should be
    // queued to another thread and not block the callback thread
    //
    //

/* todo
    ReqContext->request->postData.length() == 0 ? nullptr :
            const_cast<char*>(ReqContext->request->postData.data());
    static_cast<DWORD>(request.postData.length())


    Success = ReadFile(ReqContext->UploadFile,
                       ReqContext->OutputBuffer,
                       BUFFER_LEN,
                       &ReqContext->ReadBytes,
                       nullptr);
    if(!Success)
    {
        Error = GetLastError();
        LogSysError(Error, L"ReadFile");
        goto Exit;
    }

Exit:
*/

    return Error;
}

DWORD AsyncDownloader::PostDataToServer(PREQUEST_CONTEXT ReqContext, PBOOL Eof)
{
    DWORD Error = ERROR_SUCCESS;
    BOOL Success;

    *Eof = FALSE;

    if(ReqContext->ReadBytes == 0)
    {
        *Eof = TRUE;
        goto Exit;
    }


    Success = AcquireRequestHandle(ReqContext);
    if(!Success)
    {
        Error = ERROR_OPERATION_ABORTED;
        goto Exit;
    }


    //
    // The lpdwNumberOfBytesWritten parameter will be
    // populated on async completion, so it must exist
    // until INTERNET_STATUS_REQUEST_COMPLETE.
    // The same is true of lpBuffer parameter.
    //

    Success = InternetWriteFile(ReqContext->RequestHandle,
                                ReqContext->OutputBuffer,
                                ReqContext->ReadBytes,
                                &ReqContext->WrittenBytes);


    ReleaseRequestHandle(ReqContext);

    if(!Success)
    {
        Error = GetLastError();

        if(Error == ERROR_IO_PENDING)
        {
            qCDebug(npackd) << "Waiting for InternetWriteFile to complete";
        }
        else
        {
            LogInetError(Error, L"InternetWriteFile");
        }

        goto Exit;

    }



Exit:


    return Error;
}

DWORD AsyncDownloader::CompleteRequest(PREQUEST_CONTEXT ReqContext)
{
    DWORD Error = ERROR_SUCCESS;
    BOOL Success;

    qCDebug(npackd) << "Finished posting file";

    Success = AcquireRequestHandle(ReqContext);
    if(!Success)
    {
        Error = ERROR_OPERATION_ABORTED;
        goto Exit;
    }

    Success = HttpEndRequest(ReqContext->RequestHandle, nullptr, 0, 0);

    ReleaseRequestHandle(ReqContext);

    if(!Success)
    {
        Error = GetLastError();
        if(Error == ERROR_IO_PENDING)
        {
            qCDebug(npackd) << "Waiting for HttpEndRequest to complete";
        }
        else
        {
            LogInetError(Error, L"HttpEndRequest");
            goto Exit;
        }
    }

Exit:

    return Error;
}

DWORD AsyncDownloader::RecvResponseData(PREQUEST_CONTEXT ReqContext)
{
    DWORD Error = ERROR_SUCCESS;
    BOOL Success;

    Success = AcquireRequestHandle(ReqContext);
    if(!Success)
    {
        Error = ERROR_OPERATION_ABORTED;
        goto Exit;
    }

    //
    // The lpdwNumberOfBytesRead parameter will be
    // populated on async completion, so it must exist
    // until INTERNET_STATUS_REQUEST_COMPLETE.
    // The same is true of lpBuffer parameter.
    //
    // InternetReadFile will block until the buffer
    // is completely filled or the response is exhausted.
    //


    Success = InternetReadFile(ReqContext->RequestHandle,
                               ReqContext->OutputBuffer,
                               BUFFER_LEN,
                               &ReqContext->DownloadedBytes);

    ReleaseRequestHandle(ReqContext);

    if(!Success)
    {
        Error = GetLastError();
        if(Error == ERROR_IO_PENDING)
        {
            qCDebug(npackd) << "Waiting for InternetReadFile to complete";
        }
        else
        {
            LogInetError(Error, L"InternetReadFile");
        }

        goto Exit;
    }


Exit:

    return Error;
}

DWORD AsyncDownloader::WriteResponseData(PREQUEST_CONTEXT ReqContext, PBOOL Eof)
{
    DWORD Error = ERROR_SUCCESS;

    BOOL Success = FALSE;

     *Eof = FALSE;

    //
    // Finished receiving response
    //

    if (ReqContext->DownloadedBytes == 0)
    {
        *Eof = TRUE;
        goto Exit;

    }

    //
    //
    // WriteFile is done inline here assuming that it will return quickly
    // I.E. the file is on disk
    //
    // If you plan to do blocking/intensive operations they should be
    // queued to another thread and not block the callback thread
    //
    //

    if (ReqContext->DownloadFile)
    {
            // TODO: check the returned value. -1 means error
        DWORD BytesWritten = ReqContext->DownloadFile->write(ReqContext->OutputBuffer,
                        ReqContext->DownloadedBytes);
    }

    if (!Success)
    {
        Error = GetLastError();

        LogSysError(Error, L"WriteFile");
        goto Exit;;
    }


Exit:

    return Error;
}

void AsyncDownloader::CloseRequestHandle(PREQUEST_CONTEXT ReqContext)
{
    BOOL Close = FALSE;

    EnterCriticalSection(&ReqContext->CriticalSection);

    //
    // Current implementation only supports the main thread
    // kicking off the request handle close
    //
    // To support multiple threads the lifetime
    // of the request context must be carefully controlled
    // (most likely guarded by refcount/critsec)
    // so that they are not trying to abort a request
    // where the context has already been freed.
    //

    // ASYNC_ASSERT(ReqContext->Closing == FALSE);
    ReqContext->Closing = TRUE;

    if(ReqContext->HandleUsageCount == 0)
    {
        Close = TRUE;
    }

    LeaveCriticalSection(&ReqContext->CriticalSection);



    if(Close)
    {
        //
        // At this point there must be the guarantee that all calls
        // to wininet with this handle have returned with some value
        // including ERROR_IO_PENDING, and none will be made after
        // InternetCloseHandle.
        //
        (void)InternetCloseHandle(ReqContext->RequestHandle);
    }

    return;
}

BOOL AsyncDownloader::AcquireRequestHandle(PREQUEST_CONTEXT ReqContext)
{
    BOOL Success = TRUE;

    EnterCriticalSection(&ReqContext->CriticalSection);

    if (ReqContext->Closing == TRUE) {
        Success = FALSE;
    } else {
        ReqContext->HandleUsageCount++;
    }

    LeaveCriticalSection(&ReqContext->CriticalSection);

    return Success;
}

void AsyncDownloader::ReleaseRequestHandle(PREQUEST_CONTEXT ReqContext)
{
    BOOL Close = FALSE;

    EnterCriticalSection(&ReqContext->CriticalSection);

    // ASYNC_ASSERT(ReqContext->HandleUsageCount > 0);

    ReqContext->HandleUsageCount--;

    if(ReqContext->Closing == TRUE && ReqContext->HandleUsageCount == 0)
    {
        Close = TRUE;

    }

    LeaveCriticalSection(&ReqContext->CriticalSection);


    if(Close)
    {
        //
        // At this point there must be the guarantee that all calls
        // to wininet with this handle have returned with some value
        // including ERROR_IO_PENDING, and none will be made after
        // InternetCloseHandle.
        //
        (void)InternetCloseHandle(ReqContext->RequestHandle);
    }
}

DWORD AsyncDownloader::AllocateAndInitializeRequestContext(
        Job* job, HINTERNET SessionHandle,
        PREQUEST_CONTEXT *ReqContext,
        const Downloader::Request &request)
{
    DWORD Error = ERROR_SUCCESS;
    BOOL Success;
    PREQUEST_CONTEXT LocalReqContext;

    *ReqContext = nullptr;

    LocalReqContext = (PREQUEST_CONTEXT) malloc(sizeof(REQUEST_CONTEXT));

    if(LocalReqContext == nullptr)
    {
        Error = ERROR_NOT_ENOUGH_MEMORY;
        goto Exit;
    }

    LocalReqContext->RequestHandle = nullptr;
    LocalReqContext->ConnectHandle = nullptr;
    LocalReqContext->DownloadedBytes = 0;
    LocalReqContext->WrittenBytes = 0;
    LocalReqContext->ReadBytes = 0;
    LocalReqContext->UploadFile = INVALID_HANDLE_VALUE;
    LocalReqContext->DownloadFile = request.file;
    LocalReqContext->FileSize = 0;
    LocalReqContext->HandleUsageCount = 0;
    LocalReqContext->Closing = FALSE;
    LocalReqContext->CompletionEvent = nullptr;
    LocalReqContext->CleanUpEvent = nullptr;
    LocalReqContext->OutputBuffer = nullptr;
    if (request.httpMethod == "GET")
        LocalReqContext->State = REQ_STATE_SEND_REQ;
    else
        LocalReqContext->State = REQ_STATE_SEND_REQ_WITH_BODY;
    LocalReqContext->CritSecInitialized = FALSE;


    // initialize critical section

    Success = InitializeCriticalSectionAndSpinCount(&LocalReqContext->CriticalSection, SPIN_COUNT);

    if(!Success)
    {
        Error = GetLastError();
        LogSysError(Error, L"InitializeCriticalSectionAndSpinCount");
        goto Exit;
    }

    LocalReqContext->CritSecInitialized = TRUE;

    LocalReqContext->OutputBuffer = (LPSTR) malloc(BUFFER_LEN);

    if(LocalReqContext->OutputBuffer == nullptr)
    {
        Error = ERROR_NOT_ENOUGH_MEMORY;
        goto Exit;
    }

    // create events
    LocalReqContext->CompletionEvent = CreateEvent(nullptr,  // Sec attrib
                                                   FALSE, // Auto reset
                                                   FALSE, // Initial state unsignalled
                                                   nullptr); // Name
    if(LocalReqContext->CompletionEvent == nullptr)
    {
        Error = GetLastError();
        LogSysError(Error, L"CreateEvent CompletionEvent");
        goto Exit;
    }

    // create events
    LocalReqContext->CleanUpEvent = CreateEvent(nullptr,  // Sec attrib
                                                FALSE, // Auto reset
                                                FALSE, // Initial state unsignalled
                                                nullptr); // Name
    if(LocalReqContext->CleanUpEvent == nullptr)
    {
        Error = GetLastError();
        LogSysError(Error, L"CreateEvent CleanUpEvent");
        goto Exit;
    }

    // Verify if we've opened a file to post and get its size
    if (LocalReqContext->UploadFile != INVALID_HANDLE_VALUE)
    {
        LocalReqContext->FileSize = GetFileSize(LocalReqContext->UploadFile,
                nullptr);
        if(LocalReqContext->FileSize == INVALID_FILE_SIZE)
        {
            Error = GetLastError();
            LogSysError(Error, L"GetFileSize");
            goto Exit;
        }
    }


    Error = CreateWininetHandles(job, LocalReqContext, SessionHandle, request);

    if(Error != ERROR_SUCCESS)
    {
        qCDebug(npackd) << "CreateWininetHandles failed with" << Error;
        goto Exit;
    }


    *ReqContext = LocalReqContext;

Exit:

    if(Error != ERROR_SUCCESS)
    {
        CleanUpRequestContext(LocalReqContext);
    }

    return Error;
}

DWORD AsyncDownloader::CreateWininetHandles(Job* job, PREQUEST_CONTEXT ReqContext,
        HINTERNET SessionHandle,
        const Downloader::Request &request)
{
    QString server = request.url.host();

    // here "internet" cannot be 0, but we add the comparison to silence
    // Coverity
    HINTERNET hConnectHandle = nullptr;
    if (job->shouldProceed() && SessionHandle != nullptr) {
        // TODO: support other port numbers
        INTERNET_PORT port = static_cast<INTERNET_PORT>(
                request.url.port(request.url.scheme() == "https" ?
                INTERNET_DEFAULT_HTTPS_PORT: INTERNET_DEFAULT_HTTP_PORT));

        // For HTTP InternetConnect returns synchronously because it does not
        // actually make the connection.
        hConnectHandle = InternetConnectW(SessionHandle,
                WPMUtils::toLPWSTR(server), port, nullptr, nullptr,
                INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)ReqContext);

        if (hConnectHandle == nullptr) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
        }
    }

    ReqContext->ConnectHandle = hConnectHandle;

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
        // WinInet example uses NULL for the accepted types. Is "*/*" better?
        LPCTSTR ppszAcceptTypes[2];
        ppszAcceptTypes[0] = L"*/*";
        ppszAcceptTypes[1] = nullptr;

        DWORD flags = (request.url.scheme() == "https" ? INTERNET_FLAG_SECURE : 0);
        if (request.keepConnection)
            flags |= INTERNET_FLAG_KEEP_CONNECTION;
        if (!request.useInternet)
            flags |= INTERNET_FLAG_FROM_CACHE;
        flags |= INTERNET_FLAG_RESYNCHRONIZE;
        if (!request.useCache)
            flags |= INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_PRAGMA_NOCACHE |
                    INTERNET_FLAG_RELOAD;

        QString resource = request.url.path();
        QString encQuery = request.url.query(QUrl::FullyEncoded);
        if (!encQuery.isEmpty())
            resource.append('?').append(encQuery);

        hResourceHandle = HttpOpenRequestW(hConnectHandle,
                WPMUtils::toLPWSTR(request.httpMethod),
                WPMUtils::toLPWSTR(resource),
                nullptr, nullptr, ppszAcceptTypes,
                flags, (DWORD_PTR)ReqContext);
        if (hResourceHandle == nullptr) {
            QString errMsg;
            WPMUtils::formatMessage(GetLastError(), &errMsg);
            job->setErrorMessage(errMsg);
        }

        ReqContext->RequestHandle = hResourceHandle;
    }

    return 0; // TODO: process errors
}

void AsyncDownloader::WaitForRequestCompletion(PREQUEST_CONTEXT ReqContext, DWORD Timeout)
{
    DWORD SyncResult;

    //
    // The preferred method of doing timeouts is to
    // use the timeout options through InternetSetOption,
    // but this overall timeout is being used to show
    // the correct way to abort and close a request.
    //

    SyncResult = WaitForSingleObject(ReqContext->CompletionEvent,
                                     Timeout);              // Wait until we receive the completion

    switch(SyncResult)
    {
        case WAIT_OBJECT_0:

            qCDebug(npackd) << "Done!";
            break;

        case WAIT_TIMEOUT:

            qCDebug(npackd) << "Timeout while waiting for completion event (request will be cancelled)";
            break;

        case WAIT_FAILED:

            qCDebug(npackd) << "Wait failed with Error %d while waiting for completion event (request will be cancelled)"<<
                    GetLastError();
            break;

        default:
            // Not expecting any other error codes
            // ASYNC_ASSERT(FALSE);
            break;


    }

    return;
}

void AsyncDownloader::CleanUpRequestContext(PREQUEST_CONTEXT ReqContext)
{
    if(ReqContext == nullptr)
    {
        goto Exit;
    }

    if(ReqContext->RequestHandle)
    {
        CloseRequestHandle(ReqContext);

        //
        // Wait for the closing of the handle to complete
        // (waiting for all async operations to complete)
        //
        // This is the only safe way to get rid of the context
        //

        (void)WaitForSingleObject(ReqContext->CleanUpEvent, INFINITE);
    }

    if(ReqContext->ConnectHandle)
    {
        //
        // Remove the callback from the ConnectHandle since
        // we don't want the closing notification
        // The callback was inherited from the session handle
        //
        (void)InternetSetStatusCallback(ReqContext->ConnectHandle,
                                        NULL);

        (void)InternetCloseHandle(ReqContext->ConnectHandle);
    }

    if(ReqContext->UploadFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ReqContext->UploadFile);
    }

/*    maybe it makes sense to close request->file
 * What if the download fails?
 *
 * if(ReqContext->DownloadFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ReqContext->DownloadFile);
    }
    */

    if(ReqContext->CompletionEvent)
    {
        CloseHandle(ReqContext->CompletionEvent);
    }

    if(ReqContext->CleanUpEvent)
    {
        CloseHandle(ReqContext->CleanUpEvent);
    }

    if(ReqContext->CritSecInitialized)
    {
        DeleteCriticalSection(&ReqContext->CriticalSection);
    }

    if(ReqContext->OutputBuffer)
    {
        free(ReqContext->OutputBuffer);
    }



    free(ReqContext);


Exit:

    return;
}


void AsyncDownloader::LogInetError(DWORD Err, LPCWSTR Str)
{
    DWORD Result;
    PWSTR MsgBuffer = NULL;

    Result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_HMODULE,
                           GetModuleHandle(L"wininet.dll"),
                           Err,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPWSTR)&MsgBuffer,
                           ERR_MSG_LEN,
                           NULL);

    if (Result)
    {
        qCDebug(npackd) << "%ws: %ws" << Str << MsgBuffer;
        LocalFree(MsgBuffer);
    }
    else
    {
        qCDebug(npackd) <<
                "Error %d while formatting message for %d in %ws"<<
                GetLastError()<<
                Err<<
                Str;
    }

    return;
}

void AsyncDownloader::LogSysError(DWORD Err, LPCWSTR Str)
{
    DWORD Result;
    PWSTR MsgBuffer = NULL;

    Result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL,
                           Err,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPWSTR)&MsgBuffer,
                           ERR_MSG_LEN,
                           NULL);

    if (Result)
    {
        qCDebug(npackd) <<
                "%ws: %ws\n"<<
                Str<<
                MsgBuffer;
       LocalFree(MsgBuffer);
    }
    else
    {
        qCDebug(npackd) <<
                "Error %d while formatting message for %d in %ws"<<
                GetLastError()<<
                Err<<
                Str;
    }

    return;
}
