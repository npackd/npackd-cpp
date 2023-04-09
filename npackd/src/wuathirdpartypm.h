#ifndef WUATHIRDPARTYPM_H
#define WUATHIRDPARTYPM_H

#include "job.h"
#include "installedpackageversion.h"
#include "repository.h"
#include "abstractthirdpartypm.h"

class WUAThirdPartyPM: public AbstractThirdPartyPM
{
public:
    WUAThirdPartyPM();

    void scan(Job *job, std::vector<InstalledPackageVersion*>* installed,
              Repository* rep) const;
};

#endif // WUATHIRDPARTYPM_H
