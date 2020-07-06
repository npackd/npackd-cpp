#include <windows.h>

#include "dismapi.h"
#include "dismthirdpartypm.h"

int DISMThirdPartyPM::initializationStatus = 0;

DISMThirdPartyPM::DISMThirdPartyPM()
{
}

void DISMThirdPartyPM::scan(Job *job, QList<InstalledPackageVersion *> *installed, Repository *rep) const
{
    if (initializationStatus == 0) {
        HMODULE dismapi = LoadLibraryA("DismApi.dll");

        if (dismapi != NULL) {
            typedef HRESULT (WINAPI *LPFDISMINITIALIZE) (
                    _In_ DismLogLevel LogLevel, _In_opt_ PCWSTR LogFilePath,
                    _In_opt_ PCWSTR ScratchDirectory);

            LPFDISMINITIALIZE lpfDismInitialize =
                    (LPFDISMINITIALIZE) GetProcAddress(dismapi, "DismInitialize");

            if (lpfDismInitialize) {
                // TODO: log file
                HRESULT hr = lpfDismInitialize(DismLogErrorsWarningsInfo, L"C:\\MyLogFile.txt", NULL);
                if (FAILED(hr)) {
                    initializationStatus = 1;
                } else {
                    initializationStatus = 2;
                }
            } else {
                initializationStatus = 1;
            }
        } else {
            initializationStatus = 1;
        }
    }

    if (initializationStatus == 2) {

    }
}
