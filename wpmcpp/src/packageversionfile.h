#ifndef PACKAGEVERSIONFILE_H
#define PACKAGEVERSIONFILE_H

#include "qstring.h"

/**
 * @brief <file>
 */
class PackageVersionFile
{
public:
    /** path to the file relatively to the package version directory */
    QString path;

    /** content of the file */
    QString content;

    PackageVersionFile(const QString& path, const QString& content);

    /**
     * @return [ownership:caller] copy of this object
     */
    PackageVersionFile* clone() const;
};

#endif // PACKAGEVERSIONFILE_H
