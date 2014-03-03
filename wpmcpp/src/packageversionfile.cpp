#include "packageversionfile.h"

PackageVersionFile::PackageVersionFile(const QString& path,
        const QString& content): path(path), content(content)
{
}

PackageVersionFile *PackageVersionFile::clone() const
{
    PackageVersionFile* r = new PackageVersionFile(path, content);
    return r;
}
