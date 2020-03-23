#ifndef WUATHIRDPARTYPM_H
#define WUATHIRDPARTYPM_H

#include <QList>

#include "job.h"
#include "installedpackageversion.h"
#include "repository.h"
#include "abstractthirdpartypm.h"

class WUAThirdPartyPM: public AbstractThirdPartyPM
{
public:
    WUAThirdPartyPM();

    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
              Repository* rep) const;
};

#endif // WUATHIRDPARTYPM_H
