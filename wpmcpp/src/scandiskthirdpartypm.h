#ifndef SCANDISKTHIRDPARTYPM_H
#define SCANDISKTHIRDPARTYPM_H

#include "abstractthirdpartypm.h"

class ScanDiskThirdPartyPM: public AbstractThirdPartyPM
{
private:
    /**
     * All paths should be in lower case
     * and separated with \ and not / and cannot end with \.
     *
     * @param path directory
     * @param job job
     * @param ignore ignored directories
     * @threadsafe
     */
    void scan(const QString& path, Job* job, int level, QStringList& ignore) const;
public:
    ScanDiskThirdPartyPM();

    void scan(Job *job, QList<InstalledPackageVersion *> *installed,
            Repository *rep) const;
};

#endif // SCANDISKTHIRDPARTYPM_H
