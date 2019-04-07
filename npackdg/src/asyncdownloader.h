#ifndef ASYNCDOWNLOADER_H
#define ASYNCDOWNLOADER_H

#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <cstdint>

#include "job.h"
#include "downloader.h"


#ifdef DBG
#define ASYNC_ASSERT(x) \
    do                  \
    {                   \
        if (x)          \
        {               \
            break;      \
        }               \
        DebugBreak();   \
    }                   \
    while (FALSE, FALSE)
#else
#define ASYNC_ASSERT(x)
#endif

#define BUFFER_LEN  4096
#define ERR_MSG_LEN 512

#define METHOD_NONE 0
#define METHOD_GET  1
#define METHOD_POST 2

#define REQ_STATE_SEND_REQ             0
#define REQ_STATE_SEND_REQ_WITH_BODY   1
#define REQ_STATE_POST_GET_DATA        2
#define REQ_STATE_POST_SEND_DATA       3
#define REQ_STATE_POST_COMPLETE        4
#define REQ_STATE_RESPONSE_RECV_DATA   5
#define REQ_STATE_RESPONSE_WRITE_DATA  6
#define REQ_STATE_COMPLETE             7

#define DEFAULT_HOSTNAME L"www.microsoft.com"
#define DEFAULT_RESOURCE L"/"

#define DEFAULT_OUTPUT_FILE_NAME L"response.htm"
#define SPIN_COUNT 4000

//
// Structure to store configuration in that was gathered from
// passed in arguments
//

typedef struct _CONFIGURATION
{
    DWORD Method;                 // Method, GET or POST
    LPWSTR ResourceOnServer;      // Resource to get from the server
    LPWSTR InputFileName;         // File containing data to post
    LPWSTR OutputFileName;        // File to write the data received from the server
} CONFIGURATION, *PCONFIGURATION;

//
// Structure used for storing the context for the asynchronous calls
//

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
    HANDLE DownloadFile;
    DWORD Method;
    DWORD State;

    CRITICAL_SECTION CriticalSection;
    BOOL CritSecInitialized;


    //
    // Synchronized by CriticalSection
    //

    DWORD HandleUsageCount; // Request object is in use(not safe to close handle)
    BOOL Closing;           // Request is closing(don't use handle)

} REQUEST_CONTEXT, *PREQUEST_CONTEXT;



class AsyncDownloader
{
public:
    AsyncDownloader();

    static int64_t downloadWin(Job* job, const Downloader::Request& request,
            Downloader::Response *response);
private:
    static DWORD AllocateAndInitializeRequestContext(Job *job,
            HINTERNET SessionHandle,
            PREQUEST_CONTEXT *ReqContext,
            const Downloader::Request& request);
};

// WinInet Callback function
VOID CALLBACK
CallBack(
    HINTERNET hInternet,
    DWORD_PTR dwContext,
    DWORD dwInternetStatus,
    LPVOID lpvStatusInformation,
    DWORD dwStatusInformationLength
    );


//
// IO related functions
//

VOID ProcessRequest(PREQUEST_CONTEXT ReqContext, DWORD Error);

DWORD SendRequest(PREQUEST_CONTEXT ReqContext);

DWORD SendRequestWithBody(PREQUEST_CONTEXT ReqContext);

DWORD GetDataToPost(PREQUEST_CONTEXT ReqContext);

DWORD PostDataToServer(PREQUEST_CONTEXT ReqContext, PBOOL Eof);

DWORD CompleteRequest(PREQUEST_CONTEXT ReqContext);

DWORD RecvResponseData(PREQUEST_CONTEXT ReqContext);

DWORD WriteResponseData(PREQUEST_CONTEXT ReqContext, PBOOL Eof);


DWORD CreateWininetHandles(Job *job, PREQUEST_CONTEXT ReqContext,
        HINTERNET SessionHandle,
        LPWSTR Resource,
        const Downloader::Request &request);


//
// Cleanup functions
//

VOID CleanUpRequestContext(PREQUEST_CONTEXT ReqContext);


VOID CleanUpSessionHandle(HINTERNET SessionHandle);

//
// Cancellation support functions
//


VOID CloseRequestHandle(PREQUEST_CONTEXT ReqContext);

BOOL AcquireRequestHandle(PREQUEST_CONTEXT ReqContext);

VOID ReleaseRequestHandle(PREQUEST_CONTEXT ReqContext);


//
// Utility functions
//

void WaitForRequestCompletion(PREQUEST_CONTEXT ReqContext, DWORD Timeout);

DWORD OpenFiles(PREQUEST_CONTEXT ReqContext, DWORD Method,
    LPWSTR InputFileName, LPWSTR OutputFileName);

void LogInetError(DWORD Err, LPCWSTR Str);

void LogSysError(DWORD Err, LPCWSTR Str);

#endif // ASYNCDOWNLOADER_H
