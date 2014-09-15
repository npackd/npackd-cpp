#include <stdio.h>
#include <windows.h>
#include <string.h>

#define ERROR_EXIT_CODE 20000

/**
 * @brief full path to the current exe file
 * @return path that must be freed later or 0 if an error occures
 */
wchar_t* getExePath()
{
    // get our executable name
    DWORD sz = MAX_PATH;
    LPTSTR exe = malloc(sz * sizeof(TCHAR));
    if (GetModuleFileName(0, exe, sz) == sz) {
        free(exe);
        sz = MAX_PATH * 10;
        exe = malloc(sz * sizeof(TCHAR));
        if (GetModuleFileName(0, exe, sz) == sz) {
            exe = 0;
        }
    }

    return exe;
}

int copyExe(wchar_t **argv)
{
    int ret = 0;

    wchar_t* exe = getExePath();
    if (!exe) {
        ret = ERROR_EXIT_CODE;
        wprintf(L"Cannot determine the name of this executable\n");
    }

    if (!ret) {
        if (!CopyFile(exe, argv[2], TRUE)) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"Copying the executable failed\n");
        }
    }

    free(exe);

    HANDLE hUpdateRes = 0;
    if (!ret) {
        hUpdateRes = BeginUpdateResource(argv[2], TRUE);
        if (!hUpdateRes) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"BeginUpdateResource failed\n");
        }
    }

    // copy the icon from the target executable
    if (!ret) {
        // http://www.codeproject.com/Articles/30644/Replacing-ICON-resources-in-EXE-and-DLL-files
    }

    // set the path to the target .exe file in the resource string 1
    if (!ret) {
        size_t targetLen = wcslen(argv[3]);
        size_t bufSize = (targetLen + 16) * sizeof(wchar_t);
        wchar_t* buf = malloc(bufSize);
        memset(buf, 0, bufSize);
        buf[1] = targetLen;
        wcscpy(buf + 2, argv[3]);

        if (!UpdateResource(hUpdateRes,
                RT_STRING,
                MAKEINTRESOURCE(1),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                buf,
                bufSize)) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"UpdateResource failed\n");
        }
    }

    if (!ret) {
        if (!EndUpdateResource(hUpdateRes, FALSE)) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"EndUpdateResource failed with the error code %d\n",
                    GetLastError());
        }
    }

    return ret;
}

int wmain(int argc, wchar_t **argv)
{
    int ret = 0;

    if (argc == 4 && wcscmp(argv[1], L"exeproxy-copy") == 0) {
        ret = copyExe(argv);
        return ret;
    }

    wchar_t* exe = getExePath();
    if (!exe) {
        ret = ERROR_EXIT_CODE;
        wprintf(L"Cannot determine the name of this executable\n");
    }

    // extract parameters
    LPTSTR cl = GetCommandLine();
    LPTSTR args = 0;
    if (!ret) {
        if (*cl == L'"') {
            LPTSTR second = wcschr(cl + 1, '"');
            if (!second) {
                args = wcschr(cl, L' ');
                if (!args)
                    args = wcsdup(L"");
                else
                    args = wcsdup(args + 1);
            } else {
                args = wcsdup(second + 1);
            }
        } else {
            args = wcschr(cl, L' ');
            if (!args)
                args = wcsdup(L"");
            else
                args = wcsdup(args + 1);
        }
    }

    wchar_t* target = 0;
    if (!ret) {
        target = malloc(MAX_PATH * sizeof(TCHAR));
        if (LoadString(0, 1, target, MAX_PATH) == 0) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"Cannot load the target executable path from the resource\n");
        }
    }

    // find the name of the target executable
    wchar_t* newExe = 0;
    if (!ret) {
        // is full path to the target executable available?
        if (wcschr(target, L'\\') || wcschr(target, L'/')) {
            newExe = wcsdup(target);
        } else {
            wchar_t* p1 = wcsrchr(exe, L'\\');
            wchar_t* p2 = wcsrchr(exe, L'/');
            wchar_t* p = exe;
            if (p1 < p2)
                p = p2 + 1;
            else if (p1 > p2)
                p = p1 + 1;
            else
                p = exe;

            size_t n = p - exe;
            newExe = malloc((n + wcslen(target) + 1) * sizeof(wchar_t));
            wcsncpy(newExe, exe, n);
            *(newExe + n) = 0;

            wcscat(newExe, target);
        }
    }

    free(target);

    wchar_t* cmdLine = 0;
    if (!ret) {
        cmdLine = malloc((wcslen(newExe) + wcslen(args) + 4) * sizeof(wchar_t));
        wcscpy(cmdLine, L"\"");
        wcscat(cmdLine, newExe);
        wcscat(cmdLine, L"\"");
        if (wcslen(args) > 0) {
            wcscat(cmdLine, L" ");
            wcscat(cmdLine, args);
        }
    }

    if (!ret) {
        PROCESS_INFORMATION pinfo;

        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), 0, 0, 0,
            (DWORD) CW_USEDEFAULT, (DWORD) CW_USEDEFAULT,
            (DWORD) CW_USEDEFAULT, (DWORD) CW_USEDEFAULT,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        WINBOOL success = CreateProcess(
                newExe,
                cmdLine,
                0, 0, TRUE,
                CREATE_UNICODE_ENVIRONMENT, 0,
                0, &startupInfo, &pinfo);

        if (success) {
            WaitForSingleObject(pinfo.hProcess, INFINITE);
            DWORD ec;
            if (GetExitCodeProcess(pinfo.hProcess, &ec))
                ret = ec;
            else
                ret = ERROR_EXIT_CODE;
            CloseHandle(pinfo.hThread);
            CloseHandle(pinfo.hProcess);
        } else {
            wprintf(L"Error starting %ls %ls\n", newExe, args);
            ret = ERROR_EXIT_CODE;
        }
    }
    
    free(cmdLine);
    free(exe);
    free(newExe);
    free(args);

    return ret;
}

