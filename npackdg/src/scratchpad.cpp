//#include <windows.h>
//#include <initguid.h>
//#include <ole2.h>
//#include <mstask.h>
//#include <msterr.h>
//#include <objidl.h>
//#include <wchar.h>
//#include <stdio.h>

/* Task creation
int main2(int argc, char **argv)
{
  HRESULT hr = S_OK;
  ITaskScheduler *pITS;


  /////////////////////////////////////////////////////////////////
  // Call CoInitialize to initialize the COM library and then
  // call CoCreateInstance to get the Task Scheduler object.
  /////////////////////////////////////////////////////////////////
  hr = CoInitialize(NULL);
  if (SUCCEEDED(hr))
  {
     hr = CoCreateInstance(CLSID_CTaskScheduler,
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_ITaskScheduler,
                           (void **) &pITS);
     if (FAILED(hr))
     {
        CoUninitialize();
        return 1;
     }
  }
  else
  {
     return 1;
  }


  /////////////////////////////////////////////////////////////////
  // Call ITaskScheduler::NewWorkItem to create new task.
  /////////////////////////////////////////////////////////////////
  LPCWSTR pwszTaskName;
  ITask *pITask;
  IPersistFile *pIPersistFile;
  pwszTaskName = L"Test Task";

  hr = pITS->NewWorkItem(pwszTaskName,         // Name of task
                         CLSID_CTask,          // Class identifier
                         IID_ITask,            // Interface identifier
                         (IUnknown**)&pITask); // Address of task
                                                                                                                                                                                                                                                                                                                                                                                        //	interface


  pITS->Release();                               // Release object
  if (FAILED(hr))
  {
     CoUninitialize();
     fprintf(stderr, "Failed calling NewWorkItem, error = 0x%x\n",hr);
     return 1;
  }


  /////////////////////////////////////////////////////////////////
  // Call IUnknown::QueryInterface to get a pointer to
  // IPersistFile and IPersistFile::Save to save
  // the new task to disk.
  /////////////////////////////////////////////////////////////////

  hr = pITask->QueryInterface(IID_IPersistFile,
                              (void **)&pIPersistFile);

  pITask->Release();
  if (FAILED(hr))
  {
     CoUninitialize();
     fprintf(stderr, "Failed calling QueryInterface, error = 0x%x\n",hr);
     return 1;
  }


  hr = pIPersistFile->Save(NULL,
                           TRUE);
  pIPersistFile->Release();
  if (FAILED(hr))
  {
     CoUninitialize();
     fprintf(stderr, "Failed calling Save, error = 0x%x\n",hr);
     return 1;
  }


  CoUninitialize();
  printf("Created task.\n");
  return 0;
}
*/

/* TODO: the code is useful, but not in Npackd
void PackageVersion::registerFileHandlers()
{
    const REGSAM KEY_WOW64_64KEY = 0x0100;
    bool w64bit = WPMUtils::is64BitWindows();
    HKEY hkeyClasses;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Classes",
                0, KEY_CREATE_SUB_KEY | KEY_WRITE | (w64bit ? KEY_WOW64_64KEY : 0),
                &hkeyClasses) != ERROR_SUCCESS)
        return;

    QDir dir = getDirectory();
    for (int i = 0; i < this->fileHandlers.count(); i++) {
        FileExtensionHandler* fh = this->fileHandlers.at(i);

        QString prg = fh->program;
        prg.replace('\\', '-').replace('/', '-');
        QString progId = "Npackd-" + this->package + "-" +
                this->version.getVersionString() + "-" +
                prg;

        QString openKey = "Applications\\" + progId +
                          "\\shell\\open";
        QString commandKey = openKey + "\\command";
        HKEY hkey;
        long res = RegCreateKeyExW(hkeyClasses, (WCHAR*) commandKey.utf16(),
                              0, 0, 0, KEY_WRITE, 0, &hkey, 0);
        if (res != ERROR_SUCCESS)
            continue;

        // Windows 7: no "Open With" is shown if the path contains /
        // instead of
        QString cmd = "\"" + dir.absolutePath() + "\\" + fh->program +
                "\" \"%1\"";
        cmd.replace('/', '\\');
        RegSetValueEx(hkey, 0, 0, REG_SZ, (BYTE*) cmd.utf16(),
                cmd.length() * 2 + 2);
        RegCloseKey(hkey);

        res = RegOpenKeyEx(hkeyClasses, (WCHAR*) openKey.utf16(), 0,
                KEY_WRITE, &hkey);
        if (res == ERROR_SUCCESS) {
            QString s = fh->title;
            if (s.contains(this->getPackageTitle()))
                s = s + " (" + this->version.getVersionString() + ")";
            else
                s = s + " (" + this->getPackageTitle() + " " +
                        this->version.getVersionString() + ")";
            RegSetValueEx(hkey, L"FriendlyAppName", 0, REG_SZ, (BYTE*) s.utf16(),
                    s.length() * 2 + 2);
            RegCloseKey(hkey);
        }

        for (int j = 0; j < fh->extensions.count(); j++) {
            // OpenWithProgids does not work on Windows 7 if OpenWithList is
            // also defined:
            // http://msdn.microsoft.com/en-us/library/bb166549.aspx#2
            QString key = fh->extensions.at(j) + "\\OpenWithList\\" + progId;
            res = RegCreateKeyExW(hkeyClasses, (WCHAR*) key.utf16(),
                                       0, 0, 0, KEY_WRITE, 0, &hkey, 0);
            if (res == ERROR_SUCCESS) {
                RegCloseKey(hkey);
            }
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void PackageVersion::unregisterFileHandlers()
{
    const REGSAM KEY_WOW64_64KEY = 0x0100;
    bool w64bit = WPMUtils::is64BitWindows();
    HKEY hkeyClasses;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Classes",
                0, KEY_WRITE | (w64bit ? KEY_WOW64_64KEY : 0),
                &hkeyClasses) != ERROR_SUCCESS)
        return;

    for (int i = 0; i < this->fileHandlers.count(); i++) {
        FileExtensionHandler* fh = this->fileHandlers.at(i);

        QString prg = fh->program;
        prg.replace('\\', '-').replace('/', '-');
        QString progId = "Npackd-" + this->package + "-" +
                this->version.getVersionString() + "-" +
                prg;
        QString key = "Applications\\" + progId;
        WPMUtils::regDeleteTree(hkeyClasses, key);

        for (int j = 0; j < fh->extensions.count(); j++) {
            // OpenWithProgids does not work on Windows 7 if OpenWithList is
            // also defined:
            // http://msdn.microsoft.com/en-us/library/bb166549.aspx#2
            key = fh->extensions.at(j) + "\\OpenWithList\\" + progId;
            WPMUtils::regDeleteTree(hkeyClasses, key);
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}
*/

