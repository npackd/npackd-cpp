// see the definitions at https://github.com/Unr3alDr3am/udream-windowsinstalltool/blob/master/dismapi.h
// and https://github.com/erwan2212/DismAPI-delphi/blob/master/udismapi.pas
// and https://docs.microsoft.com/de-de/windows-hardware/manufacture/desktop/dism/dism-api-package-and-feature-functions-sample

#include <windows.h>

typedef enum _DismLogLevel {
    DismLogErrors = 0,
    DismLogErrorsWarnings,
    DismLogErrorsWarningsInfo
} DismLogLevel;

HRESULT WINAPI DismInitialize(_In_ DismLogLevel LogLevel, _In_opt_ PCWSTR LogFilePath,
        _In_opt_ PCWSTR ScratchDirectory);
