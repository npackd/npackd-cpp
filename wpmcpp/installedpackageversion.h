#ifndef INSTALLEDPACKAGEVERSION_H
#define INSTALLEDPACKAGEVERSION_H

#include <QString>

#include "version.h"

/**
 * @brief information about one installed package version
 */
class InstalledPackageVersion
{
public:
    QString directory;

    QString detectionInfo;

    /** full package version. This value should not be chaged. */
    QString package;

    /** version number. This value should not be changed. */
    Version version;

    InstalledPackageVersion(const QString& package,
            const Version& version,
            const QString& directory);

    /**
     * The value is not empty for externally installed and detected packages.
     * For example, packages detected from the MSI database have here the MSI
     * product GUID.
     *
     * MSI: "msi:{86ce85e6-dbac -3ffd-b977-e4b79f83c909}"
     * Control Panel Programs: "control-panel:Active Script 1.0"
     * @return detection information
     */
    QString getDetectionInfo() const;

    /**
     * Changes the installation path for this package. The value is not saved
     * in the registry.
     *
     * @param path installation path
     */
    void setPath(const QString& path);

    /**
     * @return true if this package version is installed
     */
    bool installed() const;

    /**
     * @return installation directory or "" if the package version is not
     *     installed
     */
    QString getDirectory() const;

    /**
     * @return [ownership:caller] copy of this object
     */
    InstalledPackageVersion* clone() const;

    /**
     * @return string representation
     */
    QString toString() const;
};

#endif // INSTALLEDPACKAGEVERSION_H
