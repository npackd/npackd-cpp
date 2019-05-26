#ifndef ASYNCDOWNLOADER_H
#define ASYNCDOWNLOADER_H

#include <windows.h>
#include <wininet.h>
#include <cstdint>

#include <QObject>

#include "job.h"
#include "downloader.h"

/*
 * @brief structure used for storing the context for the asynchronous calls
 */
typedef struct _REQUEST_CONTEXT {
    HINTERNET RequestHandle;
    HINTERNET ConnectHandle;
    HANDLE CompletionEvent;
    HANDLE CleanUpEvent;
    LPSTR OutputBuffer;
    DWORD DownloadedBytes;
    DWORD WrittenBytes;
    DWORD ReadBytes;
    HANDLE UploadFile;
    DWORD FileSize;
    QFile* DownloadFile;
    DWORD State;

    const Downloader::Request* request;
    Downloader::Response* response;

    CRITICAL_SECTION CriticalSection;
    BOOL CritSecInitialized;

    //
    // Synchronized by CriticalSection
    //

    DWORD HandleUsageCount; // Request object is in use(not safe to close handle)
    BOOL Closing;           // Request is closing(don't use handle)
} REQUEST_CONTEXT, *PREQUEST_CONTEXT;

/**
 * @brief asynchrouns WinInet
 */
class AsyncDownloader: public Downloader
{
    Q_OBJECT
public:
    /**
     * @brief constructor
     */
    //AsyncDownloader();

    /**
     * @brief HTTP request
     *
     * @param job
     * @param request
     * @param response
     * @return
     */
    static int64_t downloadWin2(Job* job, const Downloader::Request& request,
            Downloader::Response *response);

    /**
     * @brief Process the request context - Sending the request and
        receiving the response

    Arguments:
        ReqContext - Pointer to request context structure
        Error - error returned from last asynchronous call
     */
    static void ProcessRequest(PREQUEST_CONTEXT ReqContext, DWORD Error);
private:
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
    static DWORD AllocateAndInitializeRequestContext(Job *job,
            HINTERNET SessionHandle,
            PREQUEST_CONTEXT *ReqContext,
            const Downloader::Request& request);

    /*++

    Routine Description:
        Send the request using HttpSendRequest

    Arguments:
        ReqContext - Pointer to request context structure

    Return Value:
        Error code for the operation.

    --*/
    static DWORD SendRequest(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
        Send the request with entity-body using HttpSendRequestEx

    Arguments:
        ReqContext - Pointer to request context structure

    Return Value:
        Error code for the operation.

    --*/
    static DWORD SendRequestWithBody(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
        Reads data from a file

    Arguments:
        ReqContext - Pointer to request context structure

    Return Value:
        Error code for the operation.

    --*/
    static DWORD GetDataToPost(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
        Post data in the http request

    Arguments:
        ReqContext - Pointer to request context structure
        Eof - Done posting data to server

    Return Value:
        Error code for the operation.

    --*/
    static DWORD PostDataToServer(PREQUEST_CONTEXT ReqContext, PBOOL Eof);

    /*++

    Routine Description:
        Perform completion of asynchronous post.

    Arguments:
        ReqContext - Pointer to request context structure

    Return Value:
        Error Code for the operation.

    --*/
    static DWORD CompleteRequest(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
         Receive response

    Arguments:
         ReqContext - Pointer to request context structure

    Return Value:
         Error Code for the operation.

    --*/
    static DWORD RecvResponseData(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
         Write response to a file

    Arguments:
         ReqContext - Pointer to request context structure
         Eof - Done with response

    Return Value:
         Error Code for the operation.

    --*/
    static DWORD WriteResponseData(PREQUEST_CONTEXT ReqContext, PBOOL Eof);

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
    static DWORD CreateWininetHandles(Job *job, PREQUEST_CONTEXT ReqContext,
            HINTERNET SessionHandle,
            const Downloader::Request &request);

    /*++

    Routine Description:
        Used to cleanup the request context before exiting.

    Arguments:
        ReqContext - Pointer to request context structure

    Return Value:
        None.

    --*/
    static void CleanUpRequestContext(PREQUEST_CONTEXT ReqContext);

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
    static void CloseRequestHandle(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
        Acquire use of the request handle to make a wininet call
    Arguments:
        ReqContext - Pointer to Request context structure
    Return Value:
        TRUE - Success
        FALSE - Failure
    --*/
    static BOOL AcquireRequestHandle(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
        release use of the request handle
    Arguments:
        ReqContext - Pointer to Request context structure
    Return Value:
        None.

    --*/
    static void ReleaseRequestHandle(PREQUEST_CONTEXT ReqContext);

    /*++

    Routine Description:
        Wait for the request to complete or timeout to occur

    Arguments:
        ReqContext - Pointer to request context structure

    Return Value:
        None.

    --*/
    static void WaitForRequestCompletion(PREQUEST_CONTEXT ReqContext, DWORD Timeout);

    /*++

    Routine Description:
         This routine is used to log WinInet errors in human readable form.

    Arguments:
         Err - Error number obtained from GetLastError()
         Str - String pointer holding caller-context information

    Return Value:
        None.

    --*/
    static void LogInetError(DWORD Err, LPCWSTR Str);

    /*++

    Routine Description:
         This routine is used to log System Errors in human readable form.

    Arguments:
         Err - Error number obtained from GetLastError()
         Str - String pointer holding caller-context information

    Return Value:
        None.

    --*/
    static void LogSysError(DWORD Err, LPCWSTR Str);
};

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
        DWORD dwStatusInformationLength);

#endif // ASYNCDOWNLOADER_H
