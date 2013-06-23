#include <windows.h>
#include <qdebug.h>
#include <qstringlist.h>
#include <qstring.h>

#include "..\wpmcpp\repository.h"
#include "..\wpmcpp\commandline.h"
#include "..\wpmcpp\wpmutils.h"
#include "..\wpmcpp\abstractrepository.h"
#include "..\wpmcpp\dbrepository.h"
#include "..\wpmcpp\version.h"
#include "..\wpmcpp\installedpackages.h"

#include "app.h"

int main(int argc, char *argv[])
{
#if !defined(__x86_64__)
    LoadLibrary(L"exchndl.dll");
#endif
    AbstractRepository::setDefault_(DBRepository::getDefault());

    CoInitializeEx(0, COINIT_MULTITHREADED);

    qRegisterMetaType<JobState>("JobState");
    qRegisterMetaType<Version>("Version");

    InstalledPackages::getDefault()->packageName =
            "com.googlecode.windows-package-manager.NpackdCL";

    App app;

    return app.process();
}

