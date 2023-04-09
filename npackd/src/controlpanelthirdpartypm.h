#ifndef CONTROLPANELTHIRDPARTYPM_H
#define CONTROLPANELTHIRDPARTYPM_H

#include <windows.h>

#include <QString>

#include "repository.h"
#include "package.h"
#include "abstractthirdpartypm.h"
#include "windowsregistry.h"
#include "installedpackageversion.h"

/**
 * @brief control panel "Software"
 */
class ControlPanelThirdPartyPM: public AbstractThirdPartyPM
{
    void detectOneControlPanelProgram(std::vector<InstalledPackageVersion*>* installed,
            Repository* rep,
            const QString &registryPath,
            WindowsRegistry &k, const QString &keyName) const;
    void detectControlPanelProgramsFrom(
            std::vector<InstalledPackageVersion*>* installed,
            Repository* rep,
            HKEY root, const QString &path,
            bool useWoWNode) const;
public:
    /**
     * should the entries from the MSI package manager be ignored? The default
     * value is true.
     */
    bool ignoreMSIEntries;

    /**
     * @brief should the package titles be changed (e.g. package remove package
     *    version)
     */
    bool cleanPackageTitles = true;

    /**
     * @brief -
     */
    ControlPanelThirdPartyPM();

    void scan(Job *job, std::vector<InstalledPackageVersion*>* installed,
            Repository* rep) const;
};

#endif // CONTROLPANELTHIRDPARTYPM_H
