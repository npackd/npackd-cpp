#include "wuathirdpartypm.h"

#include <QLoggingCategory>

#include "wuapi.h"
#include "wpmutils.h"

using namespace std;


//const HRESULT WU_E_LEGACYSERVER=0x8024002B;
//const HRESULT WU_E_INVALID_CRITERIA=0x80240032;


WUAThirdPartyPM::WUAThirdPartyPM()
{

}

void WUAThirdPartyPM::scan(Job *job, QList<InstalledPackageVersion *> *installed, Repository *rep) const
{
    CoInitialize(NULL);
    HRESULT hr;

    IUpdateSession* iUpdate = nullptr;

    BSTR criteria = SysAllocString(L"IsInstalled=1 or IsHidden=1 or IsPresent=1");

    hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&iUpdate);
    job->checkHResult(hr);

    IUpdateSearcher* searcher = nullptr;
    if (job->shouldProceed()) {
        hr = iUpdate->CreateUpdateSearcher(&searcher);
        job->checkHResult(hr);
    }

    // qCDebug(npackd) << L"Searching for updates ..."<<endl;

    ISearchResult* results = nullptr;
    if (job->shouldProceed()) {
        hr = searcher->Search(criteria, &results);
        job->checkHResult(hr);
    }

    SysFreeString(criteria);

    job->setProgress(0.9);

    // DATE retdate;

    IUpdateCollection *updateList = nullptr;
    if (job->shouldProceed()) {
        hr = results->get_Updates(&updateList);
        job->checkHResult(hr);
    }

    LONG updateSize = 0;
    if (job->shouldProceed()) {
        hr = updateList->get_Count(&updateSize);
        job->checkHResult(hr);
    }

    if (job->shouldProceed()) {
        for (LONG i = 0; i < updateSize; i++) {
            if (!job->shouldProceed())
                break;

            IUpdate *updateItem = nullptr;

            if (job->shouldProceed()) {
                hr = updateList->get_Item(i, &updateItem);
                job->checkHResult(hr);
            }

            QString title;
            if (job->shouldProceed()) {
                BSTR updateName;
                hr = updateItem->get_Title(&updateName);
                if (FAILED(hr))
                    job->checkHResult(hr);
                else {
                    title = QString::fromUtf16(reinterpret_cast<const ushort*>(updateName));
                    SysFreeString(updateName);
                }
            }

            // updateItem->get_LastDeploymentChangeTime(&retdate);

            Package* p = new Package("package", title);

            IUpdateIdentity* identity = nullptr;

            if (job->shouldProceed()) {
                hr = updateItem->get_Identity(&identity);
            }

            QString guid;
            if (job->shouldProceed()) {
                BSTR s;
                hr = identity->get_UpdateID(&s);
                if (FAILED(hr))
                    job->checkHResult(hr);
                else {
                    guid = QString::fromUtf16(reinterpret_cast<const ushort*>(s));
                    SysFreeString(s);
                }
            }

            p->name = "wua." + guid;

            LONG ver = 0;
            if (job->shouldProceed()) {
                hr = identity->get_RevisionNumber(&ver);
                job->checkHResult(hr);
            }

            int hi = (ver & 0xffff0000) >> 16;
            int lo = ver & 0xffff;

            if (job->shouldProceed()) {
                BSTR s;
                hr = updateItem->get_SupportUrl(&s);
                if (FAILED(hr))
                    job->checkHResult(hr);
                else {
                    p->url = QString::fromUtf16(reinterpret_cast<const ushort*>(s));
                    SysFreeString(s);
                }
            }

            if (job->shouldProceed()) {
                BSTR s;
                hr = updateItem->get_Description(&s);
                if (FAILED(hr))
                    job->checkHResult(hr);
                else {
                    p->description = QString::fromUtf16(reinterpret_cast<const ushort*>(s));
                    SysFreeString(s);
                }
            }

            if (job->shouldProceed()) {
                rep->savePackage(p, true);

                InstalledPackageVersion* ipv = new InstalledPackageVersion(p->name, Version(hi, lo), "");
                ipv->detectionInfo = "wua:" + guid;
                installed->append(ipv);
            }

            delete p;

            if (updateItem) {
                updateItem->Release();
                updateItem = nullptr;
            }

            //COleDateTime odt;
            //odt.m_dt=retdate;
            //qCDebug(npackd)<<i+1<<" - "<<updateName<<"  Release Date "<< (LPCTSTR)odt.Format(_T("%A, %B %d, %Y"))<<endl;
        }
    }

    if (updateList) {
        updateList->Release();
        updateList = nullptr;
    }

    if (results) {
        results->Release();
        results = nullptr;
    }

    if (searcher) {
        searcher->Release();
        searcher = nullptr;
    }

    if (iUpdate) {
        iUpdate->Release();
        iUpdate = nullptr;
    }

    CoUninitialize();

    job->setProgress(1);

    job->complete();
}
