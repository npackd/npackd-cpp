#ifndef CONTROLPANELTHIRDPARTYPM_H
#define CONTROLPANELTHIRDPARTYPM_H

#include <windows.h>

#include <QStringList>
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
    void detectOneControlPanelProgram(QList<InstalledPackageVersion*>* installed,
            Repository* rep,
            const QString &registryPath,
            WindowsRegistry &k, const QString &keyName) const;
    void detectControlPanelProgramsFrom(
            QList<InstalledPackageVersion*>* installed,
            Repository* rep,
            HKEY root, const QString &path,
            bool useWoWNode) const;
public:
    /**
     * @brief -
     */
    ControlPanelThirdPartyPM();

    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const;
};

#endif // CONTROLPANELTHIRDPARTYPM_H
