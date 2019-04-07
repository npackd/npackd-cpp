#include <windows.h>
#include <wininet.h>

#include "asyncdownloader.h"
#include "wpmutils.h"

AsyncDownloader::AsyncDownloader()
{

}

int64_t AsyncDownloader::downloadWin(Job* job,
        const Downloader::Request& request, Downloader::Response *response)
{
    QUrl url = request.url;
    //QString verb = request.httpMethod;
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
/*
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
        fprintf(stderr, "InternetSetStatusCallback failed with INTERNET_INVALID_STATUS_CALLBACK\n");
        goto Exit;
    }



    // Initialize the ReqContext to be used in the asynchronous calls
    Error = AsyncDownloader::AllocateAndInitializeRequestContext(SessionHandle,
                                                &ReqContext,
                                                request);
    if (Error != ERROR_SUCCESS)
    {
        fprintf(stderr, "AllocateAndInitializeRequestContext failed with error %d\n", Error);
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

    CleanUpSessionHandle(SessionHandle);

    return 0;
}

/*++

Routine Description:
    Callback routine for asynchronous WinInet operations

Arguments:
     hInternet - The handle for which the callback function is called.
     dwContext - Pointer to the application defined context.
     dwInternetStatus - Status code indicating why the callback is called.
     lpvStatusInformation - Pointer to a buffer holding callback specific data.
     dwStatusInformationLength - Specifies size of lpvStatusInformation buffer.

Return Value:
    None.

--*/
void CALLBACK CallBack(HINTERNET hInternet, DWORD_PTR dwContext,
        DWORD dwInternetStatus, LPVOID lpvStatusInformation,
        DWORD dwStatusInformationLength)
{
    InternetCookieHistory cookieHistory;
    PREQUEST_CONTEXT ReqContext = (PREQUEST_CONTEXT)dwContext;

    UNREFERENCED_PARAMETER(dwStatusInformationLength);

    fprintf(stderr, "Callback Received for Handle %p \t", hInternet);

    switch(dwInternetStatus)
    {
        case INTERNET_STATUS_COOKIE_SENT:
            fprintf(stderr, "Status: Cookie found and will be sent with request\n");
            break;

        case INTERNET_STATUS_COOKIE_RECEIVED:
            fprintf(stderr, "Status: Cookie Received\n");
            break;

        case INTERNET_STATUS_COOKIE_HISTORY:

            fprintf(stderr, "Status: Cookie History\n");

            ASYNC_ASSERT(lpvStatusInformation);
            ASYNC_ASSERT(dwStatusInformationLength == sizeof(InternetCookieHistory));

            cookieHistory = *((InternetCookieHistory*)lpvStatusInformation);

            if(cookieHistory.fAccepted)
            {
                fprintf(stderr, "Cookie Accepted\n");
            }
            if(cookieHistory.fLeashed)
            {
                fprintf(stderr, "Cookie Leashed\n");
            }
            if(cookieHistory.fDowngraded)
            {
                fprintf(stderr, "Cookie Downgraded\n");
            }
            if(cookieHistory.fRejected)
            {
                fprintf(stderr, "Cookie Rejected\n");
            }


            break;

        case INTERNET_STATUS_CLOSING_CONNECTION:
            fprintf(stderr, "Status: Closing Connection\n");
            break;

        case INTERNET_STATUS_CONNECTED_TO_SERVER:
            fprintf(stderr, "Status: Connected to Server\n");
            break;

        case INTERNET_STATUS_CONNECTING_TO_SERVER:
            fprintf(stderr, "Status: Connecting to Server\n");
            break;

        case INTERNET_STATUS_CONNECTION_CLOSED:
            fprintf(stderr, "Status: Connection Closed\n");
            break;

        case INTERNET_STATUS_HANDLE_CLOSING:
            fprintf(stderr, "Status: Handle Closing\n");

            //
            // Signal the cleanup routine that it is
            // safe to cleanup the request context
            //

            ASYNC_ASSERT(ReqContext);
            SetEvent(ReqContext->CleanUpEvent);

            break;

        case INTERNET_STATUS_HANDLE_CREATED:
            ASYNC_ASSERT(lpvStatusInformation);
            fprintf(stderr,
                    "Handle %x created\n",
                    ((LPINTERNET_ASYNC_RESULT)lpvStatusInformation)->dwResult);

            break;

        case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
            fprintf(stderr, "Status: Intermediate response\n");
            break;

        case INTERNET_STATUS_RECEIVING_RESPONSE:
            fprintf(stderr, "Status: Receiving Response\n");
            break;

        case INTERNET_STATUS_RESPONSE_RECEIVED:
            ASYNC_ASSERT(lpvStatusInformation);
            ASYNC_ASSERT(dwStatusInformationLength == sizeof(DWORD));

            fprintf(stderr, "Status: Response Received (%d Bytes)\n",
                *((LPDWORD)lpvStatusInformation));

            break;

        case INTERNET_STATUS_REDIRECT:
            fprintf(stderr, "Status: Redirect\n");
            break;

        case INTERNET_STATUS_REQUEST_COMPLETE:
            fprintf(stderr, "Status: Request complete\n");

            ASYNC_ASSERT(lpvStatusInformation);

            ProcessRequest(ReqContext, ((LPINTERNET_ASYNC_RESULT)lpvStatusInformation)->dwError);

            break;

        case INTERNET_STATUS_REQUEST_SENT:
            ASYNC_ASSERT(lpvStatusInformation);
            ASYNC_ASSERT(dwStatusInformationLength == sizeof(DWORD));

            fprintf(stderr,"Status: Request sent (%d Bytes)\n", *((LPDWORD)lpvStatusInformation));
            break;

        case INTERNET_STATUS_DETECTING_PROXY:
            fprintf(stderr, "Status: Detecting Proxy\n");
            break;

        case INTERNET_STATUS_RESOLVING_NAME:
            fprintf(stderr, "Status: Resolving Name\n");
            break;

        case INTERNET_STATUS_NAME_RESOLVED:
            fprintf(stderr, "Status: Name Resolved\n");
            break;

        case INTERNET_STATUS_SENDING_REQUEST:
            fprintf(stderr, "Status: Sending request\n");
            break;

        case INTERNET_STATUS_STATE_CHANGE:
            fprintf(stderr, "Status: State Change\n");
            break;

        case INTERNET_STATUS_P3P_HEADER:
            fprintf(stderr, "Status: Received P3P header\n");
            break;

        default:
            fprintf(stderr, "Status: Unknown (%d)\n", dwInternetStatus);
            break;
    }

    return;
}


/*++

Routine Description:
    Process the request context - Sending the request and
    receiving the response

Arguments:
    ReqContext - Pointer to request context structure
    Error - error returned from last asynchronous call

Return Value:
    None.

--*/
void ProcessRequest(PREQUEST_CONTEXT ReqContext, DWORD Error)
{
    BOOL Eof = FALSE;

    ASYNC_ASSERT(ReqContext);


    while(Error == ERROR_SUCCESS && ReqContext->State != REQ_STATE_COMPLETE)
    {

        switch(ReqContext->State)
        {
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

                if(Eof)
                {
                    ASYNC_ASSERT(Error == ERROR_SUCCESS);
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

                if(Eof)
                {
                    ASYNC_ASSERT(Error == ERROR_SUCCESS);
                    ReqContext->State = REQ_STATE_COMPLETE;
                }

                break;

            default:

                ASYNC_ASSERT(FALSE);

                break;
        }
    }

    if(Error != ERROR_IO_PENDING)
    {
        //
        // Everything has been procesed or has failed.
        // In either case, the signal processing has
        // completed
        //

        SetEvent(ReqContext->CompletionEvent);
    }

    return;
}

/*++

Routine Description:
    Send the request using HttpSendRequest

Arguments:
    ReqContext - Pointer to request context structure

Return Value:
    Error code for the operation.

--*/
DWORD SendRequest(PREQUEST_CONTEXT ReqContext)
{
    BOOL Success;
    DWORD Error = ERROR_SUCCESS;


    ASYNC_ASSERT(ReqContext->Method == METHOD_GET);

    Success = AcquireRequestHandle(ReqContext);
    if(!Success)
    {
        Error = ERROR_OPERATION_ABORTED;
        goto Exit;
    }

    Success = HttpSendRequest(ReqContext->RequestHandle,
                              NULL,                   // do not provide additional Headers
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

/*++

Routine Description:
    Send the request with entity-body using HttpSendRequestEx

Arguments:
    ReqContext - Pointer to request context structure

Return Value:
    Error code for the operation.

--*/
DWORD SendRequestWithBody(PREQUEST_CONTEXT ReqContext)
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


    ASYNC_ASSERT(ReqContext->Method == METHOD_POST);

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

/*++

Routine Description:
    Reads data from a file

Arguments:
    ReqContext - Pointer to request context structure

Return Value:
    Error code for the operation.

--*/
DWORD GetDataToPost(PREQUEST_CONTEXT ReqContext)
{
    DWORD Error = ERROR_SUCCESS;
    BOOL Success;


    //
    //
    // ReadFile is done inline here assuming that it will return quickly
    // I.E. the file is on disk
    //
    // If you plan to do blocking/intensive operations they should be
    // queued to another thread and not block the callback thread
    //
    //

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

    return Error;
}

/*++

Routine Description:
    Post data in the http request

Arguments:
    ReqContext - Pointer to request context structure
    Eof - Done posting data to server

Return Value:
    Error code for the operation.

--*/
DWORD PostDataToServer(PREQUEST_CONTEXT ReqContext, PBOOL Eof)
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
            fprintf(stderr, "Waiting for InternetWriteFile to complete\n");
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


/*++

Routine Description:
    Perform completion of asynchronous post.

Arguments:
    ReqContext - Pointer to request context structure

Return Value:
    Error Code for the operation.

--*/
DWORD CompleteRequest(PREQUEST_CONTEXT ReqContext)
{
    DWORD Error = ERROR_SUCCESS;
    BOOL Success;

    fprintf(stderr, "Finished posting file\n");

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
            fprintf(stderr, "Waiting for HttpEndRequest to complete \n");
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

/*++

Routine Description:
     Receive response

Arguments:
     ReqContext - Pointer to request context structure

Return Value:
     Error Code for the operation.

--*/
DWORD RecvResponseData(PREQUEST_CONTEXT ReqContext)
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
            fprintf(stderr, "Waiting for InternetReadFile to complete\n");
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


/*++

Routine Description:
     Write response to a file

Arguments:
     ReqContext - Pointer to request context structure
     Eof - Done with response

Return Value:
     Error Code for the operation.

--*/
DWORD WriteResponseData(PREQUEST_CONTEXT ReqContext, PBOOL Eof)
{
    DWORD Error = ERROR_SUCCESS;
    DWORD BytesWritten = 0;

    BOOL Success;

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

    Success = WriteFile(ReqContext->DownloadFile,
                        ReqContext->OutputBuffer,
                        ReqContext->DownloadedBytes,
                        &BytesWritten,
                        nullptr);

    if (!Success)
    {
        Error = GetLastError();

        LogSysError(Error, L"WriteFile");
        goto Exit;;
    }


Exit:

    return Error;
}

/*++

Routine Description:
    Safely  close the request handle by synchronizing
    with all threads using the handle.

    When this function returns no more calls can be made with the
    handle.

Arguments:
    ReqContext - Pointer to Request context structure
Return Value:
    None.

--*/
void CloseRequestHandle(PREQUEST_CONTEXT ReqContext)
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

    ASYNC_ASSERT(ReqContext->Closing == FALSE);
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
        (VOID)InternetCloseHandle(ReqContext->RequestHandle);
    }

    return;
}

/*++

Routine Description:
    Acquire use of the request handle to make a wininet call
Arguments:
    ReqContext - Pointer to Request context structure
Return Value:
    TRUE - Success
    FALSE - Failure
--*/
BOOL AcquireRequestHandle(PREQUEST_CONTEXT ReqContext)
{
    BOOL Success = TRUE;

    EnterCriticalSection(&ReqContext->CriticalSection);

    if(ReqContext->Closing == TRUE)
    {
        Success = FALSE;
    }
    else
    {
        ReqContext->HandleUsageCount++;
    }

    LeaveCriticalSection(&ReqContext->CriticalSection);

    return Success;
}

/*++

Routine Description:
    release use of the request handle
Arguments:
    ReqContext - Pointer to Request context structure
Return Value:
    None.

--*/
void ReleaseRequestHandle(PREQUEST_CONTEXT ReqContext)
{
    BOOL Close = FALSE;

    EnterCriticalSection(&ReqContext->CriticalSection);

    ASYNC_ASSERT(ReqContext->HandleUsageCount > 0);

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
        (VOID)InternetCloseHandle(ReqContext->RequestHandle);
    }

    return;
}

/*++

Routine Description:
    Allocate the request context and initialize it values

Arguments:
    ReqContext - Pointer to Request context structure
    Configuration - Pointer to configuration structure
    SessionHandle - Wininet session handle to use when creating
                    connect handle

Return Value:
    Error Code for the operation.

--*/
DWORD AsyncDownloader::AllocateAndInitializeRequestContext(HINTERNET SessionHandle,
    PREQUEST_CONTEXT *ReqContext,
    const Downloader::Request &request)
{
    CONFIGURATION Configuration = {
        METHOD_GET,
        L"/",
        nullptr,
        L"abc.htm",
        TRUE
    };

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
    LocalReqContext->DownloadFile = INVALID_HANDLE_VALUE;
    LocalReqContext->FileSize = 0;
    LocalReqContext->HandleUsageCount = 0;
    LocalReqContext->Closing = FALSE;
    LocalReqContext->Method = Configuration.Method;
    LocalReqContext->CompletionEvent = nullptr;
    LocalReqContext->CleanUpEvent = nullptr;
    LocalReqContext->OutputBuffer = nullptr;
    LocalReqContext->State =
        (LocalReqContext->Method == METHOD_GET) ?  REQ_STATE_SEND_REQ : REQ_STATE_SEND_REQ_WITH_BODY;
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

    // Open the file to dump the response entity body and
    // if required the file with the data to post
    Error = OpenFiles(LocalReqContext,
                      Configuration.Method,
                      Configuration.InputFileName,
                      Configuration.OutputFileName);

    if(Error != ERROR_SUCCESS)
    {
            fprintf(stderr, "OpenFiles failed with %d\n", Error);
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


    Error = CreateWininetHandles(LocalReqContext,
                                 SessionHandle,
                                 Configuration.ResourceOnServer,
                                 Configuration.IsSecureConnection,
                                 request);

    if(Error != ERROR_SUCCESS)
    {
        fprintf(stderr, "CreateWininetHandles failed with %d\n", Error);
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

/*++

Routine Description:
    Create connect and request handles

Arguments:
    ReqContext - Pointer to Request context structure
    SessionHandle - Wininet session handle used to create
                    connect handle
    HostName - Hostname to connect
    Resource - Resource to get/post
    IsSecureConnection - SSL?

Return Value:
    Error Code for the operation.

--*/
DWORD CreateWininetHandles(PREQUEST_CONTEXT ReqContext,
        HINTERNET SessionHandle,
        LPWSTR Resource, BOOL IsSecureConnection,
        const Downloader::Request &request)
{
    DWORD Error = ERROR_SUCCESS;
    INTERNET_PORT ServerPort = INTERNET_DEFAULT_HTTP_PORT;
    DWORD RequestFlags = 0;
    LPWSTR Verb;


    //
    // Set the correct server port if using SSL
    // Also set the flag for HttpOpenRequest
    //

    if(IsSecureConnection)
    {
        ServerPort = INTERNET_DEFAULT_HTTPS_PORT;
        RequestFlags = INTERNET_FLAG_SECURE;
    }

    QString server = request.url.host();

    // Create Connection handle and provide context for async operations
    ReqContext->ConnectHandle = InternetConnect(SessionHandle,
                                                WPMUtils::toLPWSTR(server),                  // Name of the server to connect to
                                                ServerPort,                // HTTP (80) or HTTPS (443)
                                                nullptr,                      // Do not provide a user name for the server
                                                nullptr,                      // Do not provide a password for the server
                                                INTERNET_SERVICE_HTTP,
                                                0,                         // Do not provide any special flag
                                                (DWORD_PTR)ReqContext);    // Provide the context to be
                                                                           // used during the callbacks
    //
    // For HTTP InternetConnect returns synchronously because it does not
    // actually make the connection.
    //
    // For FTP InternetConnect connects the control channel, and therefore
    // can be completed asynchronously.  This sample would have to be
    // changed, so that the InternetConnect's asynchronous completion
    // is handled correctly to support FTP.
    //

    if (ReqContext->ConnectHandle == nullptr)
    {
        Error = GetLastError();
        LogInetError(Error, L"InternetConnect");
        goto Exit;
    }


    // Set the Verb depending on the operation to perform
    if(ReqContext->Method == METHOD_GET)
    {
        Verb = L"GET";
    }
    else
    {
        ASYNC_ASSERT(ReqContext->Method == METHOD_POST);

        Verb = L"POST";
    }

    //
    // We're overriding WinInet's default behavior.
    // Setting these flags, we make sure we get the response from the server and not the cache.
    // Also ask WinInet not to store the response in the cache.
    //
    // These flags are NOT performant and are only used to show case WinInet's Async I/O.
    // A real WinInet application would not want to use this flags.
    //

    RequestFlags |= INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;

    // Create a Request handle
    ReqContext->RequestHandle = HttpOpenRequest(ReqContext->ConnectHandle,
                                                Verb,                     // GET or POST
                                                Resource,                 // root "/" by default
                                                NULL,                     // Use default HTTP/1.1 as the version
                                                NULL,                     // Do not provide any referrer
                                                NULL,                     // Do not provide Accept types
                                                RequestFlags,
                                                (DWORD_PTR)ReqContext);

    if (ReqContext->RequestHandle == NULL)
    {
        Error = GetLastError();
        LogInetError(Error, L"HttpOpenRequest");

        goto Exit;
    }


Exit:

    return Error;
}

/*++

Routine Description:
    Wait for the request to complete or timeout to occur

Arguments:
    ReqContext - Pointer to request context structure

Return Value:
    None.

--*/
void WaitForRequestCompletion(PREQUEST_CONTEXT ReqContext, DWORD Timeout)
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

            printf("Done!\n");
            break;

        case WAIT_TIMEOUT:

            fprintf(stderr,
                    "Timeout while waiting for completion event (request will be cancelled)\n");
            break;

        case WAIT_FAILED:

            fprintf(stderr,
                    "Wait failed with Error %d while waiting for completion event (request will be cancelled)\n",
                    GetLastError());
            break;

        default:
            // Not expecting any other error codes
            ASYNC_ASSERT(FALSE);
            break;


    }

    return;
}

/*++

Routine Description:
    Used to cleanup the request context before exiting.

Arguments:
    ReqContext - Pointer to request context structure

Return Value:
    None.

--*/
void CleanUpRequestContext(PREQUEST_CONTEXT ReqContext)
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

        (VOID)WaitForSingleObject(ReqContext->CleanUpEvent, INFINITE);
    }

    if(ReqContext->ConnectHandle)
    {
        //
        // Remove the callback from the ConnectHandle since
        // we don't want the closing notification
        // The callback was inherited from the session handle
        //
        (VOID)InternetSetStatusCallback(ReqContext->ConnectHandle,
                                        NULL);

        (VOID)InternetCloseHandle(ReqContext->ConnectHandle);
    }

    if(ReqContext->UploadFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ReqContext->UploadFile);
    }

    if(ReqContext->DownloadFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ReqContext->DownloadFile);
    }

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


/*++

Routine Description:
    Used to cleanup session before exiting.

Arguments:
    SessionHandle - Wininet session handle

Return Value:
    None.

--*/
void CleanUpSessionHandle(HINTERNET SessionHandle)
{

    if(SessionHandle)
    {
        //
        // Remove the callback from the SessionHandle since
        // we don't want the closing notification
        //
        (VOID)InternetSetStatusCallback(SessionHandle,
                                        NULL);
        //
        // Call InternetCloseHandle and do not wait for the closing notification
        // in the callback function
        //
        (VOID)InternetCloseHandle(SessionHandle);

    }

    return;
}

/*++

Routine Description:
    This routine opens files, one to post data from, and
    one to write response into

Arguments:
    ReqContext - Pointer to request context structure
    Method - GET or POST - do we need to open the input file
    InputFileName - Input file name
    OutputFileName - output file name

Return Value:
    Error Code for the operation.

--*/
DWORD OpenFiles(PREQUEST_CONTEXT ReqContext, DWORD Method,
    LPWSTR InputFileName, LPWSTR OutputFileName)
{
    DWORD Error = ERROR_SUCCESS;

    if (Method == METHOD_POST)
    {
        // Open input file
        ReqContext->UploadFile = CreateFile(InputFileName,
                                            GENERIC_READ,
                                            FILE_SHARE_READ,
                                            nullptr,                   // handle cannot be inherited
                                            OPEN_ALWAYS,            // if file exists, open it
                                            FILE_ATTRIBUTE_NORMAL,
                                            nullptr);                  // No template file

        if (ReqContext->UploadFile == INVALID_HANDLE_VALUE)
        {
            Error = GetLastError();
            LogSysError(Error, L"CreateFile for input file");
            goto Exit;
        }
    }

    // Open output file
    ReqContext->DownloadFile = CreateFile(OutputFileName,
                                          GENERIC_WRITE,
                                          0,                        // Open exclusively
                                          nullptr,                     // handle cannot be inherited
                                          CREATE_ALWAYS,            // if file exists, delete it
                                          FILE_ATTRIBUTE_NORMAL,
                                          nullptr);                    // No template file

    if (ReqContext->DownloadFile == INVALID_HANDLE_VALUE)
    {
            Error = GetLastError();
            LogSysError(Error, L"CreateFile for output file");
            goto Exit;
    }

Exit:
    return Error;
}


/*++

Routine Description:
     This routine is used to log WinInet errors in human readable form.

Arguments:
     Err - Error number obtained from GetLastError()
     Str - String pointer holding caller-context information

Return Value:
    None.

--*/
VOID
LogInetError(
    DWORD Err,
    LPCWSTR Str
    )
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
        fprintf(stderr, "%ws: %ws\n", Str, MsgBuffer);
        LocalFree(MsgBuffer);
    }
    else
    {
        fprintf(stderr,
                "Error %d while formatting message for %d in %ws\n",
                GetLastError(),
                Err,
                Str);
    }

    return;
}

/*++

Routine Description:
     This routine is used to log System Errors in human readable form.

Arguments:
     Err - Error number obtained from GetLastError()
     Str - String pointer holding caller-context information

Return Value:
    None.

--*/
VOID
LogSysError(
    DWORD Err,
    LPCWSTR Str
    )
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
        fprintf(stderr,
                "%ws: %ws\n",
                Str,
                MsgBuffer);
       LocalFree(MsgBuffer);
    }
    else
    {
        fprintf(stderr,
                "Error %d while formatting message for %d in %ws\n",
                GetLastError(),
                Err,
                Str);
    }

    return;
}
