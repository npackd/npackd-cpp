#ifndef MSITHIRDPARTYPM_H
#define MSITHIRDPARTYPM_H

#include "abstractthirdpartypm.h"
#include "installedpackageversion.h"
#include "repository.h"

/**
 * @brief MSI package database
 */
class MSIThirdPartyPM: public AbstractThirdPartyPM
{
public:
    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const;
};

#endif // MSITHIRDPARTYPM_H
