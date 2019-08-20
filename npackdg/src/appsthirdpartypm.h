#ifndef APPSTHIRDPARTYPM_H
#define APPSTHIRDPARTYPM_H

#include "abstractthirdpartypm.h"
#include "installedpackageversion.h"
#include "repository.h"

/**
 * @brief package database for Windows Store apps
 */
class AppsThirdPartyPM
{
public:
    AppsThirdPartyPM();

    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const;
};

#endif // APPSTHIRDPARTYPM_H
