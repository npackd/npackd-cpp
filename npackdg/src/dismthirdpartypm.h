#ifndef DISMTHIRDPARTYPM_H
#define DISMTHIRDPARTYPM_H

#include "abstractthirdpartypm.h"

/**
 * @brief DISM
 */
class DISMThirdPartyPM: AbstractThirdPartyPM
{
    /**
     * 0 - DLL not loaded, DismInitialize not called
     * 1 - DLL loaded, DismInitialize failed
     * 2 - DLL loaded, DismInitialize succeeded
     */
    static int initializationStatus;
public:
    DISMThirdPartyPM();

    void scan(Job* job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const;
};

#endif // DISMTHIRDPARTYPM_H
